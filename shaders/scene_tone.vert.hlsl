struct Output {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};
Output main(uint id : SV_VertexID) {
  float2 uv = float2((id << 1) & 2, id & 2);
  Output o;
  o.position = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
  o.uv = uv;
  return o;
}
