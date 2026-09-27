// SDL GPU vertex uniforms occupy space1. CPU matrices are column-major.
cbuffer View : register(b0, space1) {
  column_major float4x4 modelViewProjection;
};

struct VertexInput {
  float3 position : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float2 uv : TEXCOORD2;
  float4 color : TEXCOORD3;
};

struct VertexOutput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  float4 color : TEXCOORD1;
};

VertexOutput main(VertexInput input) {
  VertexOutput output;
  output.position = mul(modelViewProjection, float4(input.position, 1.0));
  output.uv = input.uv;
  output.color = input.color;
  return output;
}
