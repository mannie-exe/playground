Texture2D<float4> baseMap : register(t0, space2);
Texture2D<float4> mrMap : register(t1, space2);
Texture2D<float4> normalMap : register(t2, space2);
Texture2D<float4> aoMap : register(t3, space2);
Texture2D<float4> emissiveMap : register(t4, space2);
Texture2D<float4> diffuseMap : register(t5, space2);
Texture2D<float4> specularMap : register(t6, space2);
Texture2D<float4> brdfMap : register(t7, space2);
SamplerState baseSampler : register(s0, space2);
SamplerState mrSampler : register(s1, space2);
SamplerState normalSampler : register(s2, space2);
SamplerState aoSampler : register(s3, space2);
SamplerState emissiveSampler : register(s4, space2);
SamplerState diffuseSampler : register(s5, space2);
SamplerState specularSampler : register(s6, space2);
SamplerState brdfSampler : register(s7, space2);
cbuffer Material : register(b0, space3) {
  float4 baseFactor, factors, emissiveFlags, alphaOptions;
  float4 eyeEnvironment, lightDirection, lightColor;
  float4 uvRows[10];
};
struct Input {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float4 tangent : TEXCOORD2;
  float2 uv : TEXCOORD3;
  float2 uv1 : TEXCOORD4;
  float4 color : TEXCOORD5;
};
float2 uv(Input i, int slot) {
  float2 p = uvRows[slot * 2].w > .5 ? i.uv1 : i.uv;
  return float2(dot(uvRows[slot * 2].xy, p) + uvRows[slot * 2].z,
                dot(uvRows[slot * 2 + 1].xy, p) + uvRows[slot * 2 + 1].z);
}
float2 environmentUV(float3 d) {
  return float2(atan2(d.x, d.z) / 6.28318530718 + .5,
                acos(clamp(d.y, -1, 1)) / 3.14159265359);
}
float3 fresnel(float cosine, float3 f0) {
  return f0 + (1 - f0) * pow(1 - saturate(cosine), 5);
}
float3 safeNormalize(float3 value, float3 fallback) {
  float squared = dot(value, value);
  return squared > 1e-12 ? value * rsqrt(squared) : fallback;
}
float4 main(Input i, bool front : SV_IsFrontFace) : SV_Target0 {
  float4 base = baseMap.Sample(baseSampler, uv(i, 0));
  base.rgb = base.a > 0 ? base.rgb / base.a : 0;
  base *= baseFactor * i.color;
  if (alphaOptions.x == 1 && base.a < alphaOptions.y)
    discard;
  if (alphaOptions.x < 2)
    base.a = 1;
  if (emissiveFlags.w < .5)
    return float4(base.rgb * base.a, base.a);
  float3 n = safeNormalize(i.normal, float3(0, 0, 1));
  if (!front)
    n = -n;
  float3 v = safeNormalize(eyeEnvironment.xyz - i.world, n),
         l = normalize(lightDirection.xyz);
  if (alphaOptions.z > .5) {
    float3 fallbackT =
        normalize(cross(abs(n.z) < .99 ? float3(0, 0, 1) : float3(0, 1, 0), n));
    float3 t =
        safeNormalize(i.tangent.xyz - n * dot(n, i.tangent.xyz), fallbackT);
    float3 b = cross(n, t) * i.tangent.w * lightDirection.w;
    float3 mapped = normalMap.Sample(normalSampler, uv(i, 2)).xyz * 2 - 1;
    mapped.xy *= factors.z;
    // UV transform rotates/scales the normal-map domain, not mesh UV data.
    float2x2 m = float2x2(uvRows[4].xy, uvRows[5].xy);
    float determinant = m._11 * m._22 - m._12 * m._21;
    if (abs(determinant) > 1e-8) {
      float3 tt = (t * m._22 - b * m._21) / determinant;
      float3 bb = (-t * m._12 + b * m._11) / determinant;
      t = safeNormalize(tt, t);
      b = safeNormalize(bb, b);
    }
    n = safeNormalize(t * mapped.x + b * mapped.y + n * mapped.z, n);
  }
  float4 mr = mrMap.Sample(mrSampler, uv(i, 1));
  float metal = saturate(factors.x * mr.b),
        rough = clamp(factors.y * mr.g, .045, 1);
  float3 h = safeNormalize(v + l, n);
  float nv = max(dot(n, v), .0001), nl = max(dot(n, l), 0),
        nh = max(dot(n, h), 0), vh = max(dot(v, h), 0);
  float a = rough * rough, a2 = a * a;
  float denominator = nh * nh * (a2 - 1) + 1;
  float d = a2 / (3.14159265359 * denominator * denominator);
  float visibility = .5 / max(nl * sqrt(nv * nv * (1 - a2) + a2) +
                                  nv * sqrt(nl * nl * (1 - a2) + a2),
                              .0001);
  float3 f0 = lerp(.04.xxx, base.rgb, metal), f = fresnel(vh, f0);
  float3 radiance =
      ((1 - f) * (1 - metal) * base.rgb / 3.14159265359 + d * visibility * f) *
      lightColor.rgb * nl;
  float ao = lerp(1, aoMap.Sample(aoSampler, uv(i, 3)).r, factors.w);
  float3 diffuse =
      diffuseMap.SampleLevel(diffuseSampler, environmentUV(n), 0).rgb;
  float3 specular =
      specularMap
          .SampleLevel(specularSampler, environmentUV(reflect(-v, n)),
                       rough * alphaOptions.w)
          .rgb;
  float2 brdf = brdfMap.SampleLevel(brdfSampler, float2(nv, rough), 0).rg;
  float3 envF = fresnel(nv, f0);
  radiance += ((1 - envF) * (1 - metal) * base.rgb * diffuse +
               specular * (f0 * brdf.x + brdf.y)) *
              eyeEnvironment.w * ao;
  radiance +=
      emissiveMap.Sample(emissiveSampler, uv(i, 4)).rgb * emissiveFlags.rgb;
  return float4(radiance * base.a, base.a);
}
