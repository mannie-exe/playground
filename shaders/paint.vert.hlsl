struct Shape { float4 rowX, rowY, bounds, top, bottom; };
struct PaintData {
    float4 tint, borderTint, sourceRect, options, imageOptions;
    Shape outer, inner;
    Shape clips[32];
};
struct DrawData { float4 bounds; PaintData paint; };
StructuredBuffer<DrawData> draws : register(t0, space0);
cbuffer Target : register(b0, space1) { float4 target; };
struct Output {
    float4 position : SV_Position;
    nointerpolation uint drawIndex : TEXCOORD0;
};
Output main(uint id : SV_VertexID, uint instance : SV_InstanceID) {
    const float2 positions[6] = {float2(0,0), float2(1,0), float2(0,1),
                                 float2(0,1), float2(1,0), float2(1,1)};
    uint drawIndex = instance + uint(target.z);
    float4 bounds = draws[drawIndex].bounds;
    float2 pixel = bounds.xy + positions[id] * bounds.zw;
    Output output;
    output.position = float4(pixel.x / target.x * 2 - 1, 1 - pixel.y / target.y * 2, 0, 1);
    output.drawIndex = drawIndex;
    return output;
}
