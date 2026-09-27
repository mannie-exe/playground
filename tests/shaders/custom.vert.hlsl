float4 main(uint id : SV_VertexID) : SV_Position {
  float2 position = float2((id << 1) & 2, id & 2);
  return float4(position * 2.0 - 1.0, 0.0, 1.0);
}
