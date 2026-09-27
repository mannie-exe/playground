cbuffer View : register(b0, space1) {
  column_major float4x4 mvp;
  column_major float4x4 model;
  column_major float4x4 normalMatrix;
};
struct Input {
  float3 position : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float2 uv : TEXCOORD2;
  float4 tangent : TEXCOORD3;
  float4 color : TEXCOORD4;
  float2 uv1 : TEXCOORD5;
};
struct Output {
  float4 position : SV_Position;
  float3 world : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float4 tangent : TEXCOORD2;
  float2 uv : TEXCOORD3;
  float2 uv1 : TEXCOORD4;
  float4 color : TEXCOORD5;
};
Output main(Input v) {
  Output o;
  o.position = mul(mvp, float4(v.position, 1));
  o.world = mul(model, float4(v.position, 1)).xyz;
  o.normal = mul(normalMatrix, float4(v.normal, 0)).xyz;
  o.tangent = float4(mul(model, float4(v.tangent.xyz, 0)).xyz, v.tangent.w);
  o.uv = v.uv;
  o.uv1 = v.uv1;
  o.color = v.color;
  return o;
}
