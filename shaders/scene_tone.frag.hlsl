Texture2D<float4> image : register(t0, space2);
SamplerState imageSampler : register(s0, space2);
cbuffer Settings : register(b0, space3) { float4 settings; };
struct Input {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};
// Khronos PBR Neutral, Apache-2.0 (see assets/demo3d/README.md).
float3 neutral(float3 color) {
  const float startCompression = .8 - .04, desaturation = .15;
  float x = min(color.r, min(color.g, color.b));
  float offset = x < .08 ? x - 6.25 * x * x : .04;
  color -= offset;
  float peak = max(color.r, max(color.g, color.b));
  if (peak < startCompression)
    return color;
  float d = 1 - startCompression,
        newPeak = 1 - d * d / (peak + d - startCompression);
  color *= newPeak / peak;
  return lerp(color, newPeak.xxx,
              1 - 1 / (desaturation * (peak - newPeak) + 1));
}
float4 main(Input i) : SV_Target0 {
  float4 c = image.SampleLevel(imageSampler, i.uv, 0);
  float3 straight = c.a > 0 ? max(c.rgb / c.a, 0) : 0;
  straight *= settings.x;
  if (settings.y > .5)
    straight = neutral(straight);
  return float4(straight * c.a, c.a);
}
