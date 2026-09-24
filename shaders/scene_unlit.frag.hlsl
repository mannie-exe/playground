// Decode texels before filtering; tint and interpolation are linear-light.
// The shader emits premultiplied linear RGBA for ONE / ONE_MINUS_SRC_ALPHA.
Texture2D<float4> baseColorImage : register(t0, space2);
SamplerState baseColorSampler : register(s0, space2);

cbuffer Material : register(b0, space3) {
    float4 linearTint;
    // premultiplied, sRGB, alpha mode (opaque/mask/blend), cutoff
    float4 options;
    // linear filtering, U address (clamp/repeat/mirror), V address, reserved
    float4 sampling;
};

struct FragmentInput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
int address(int coordinate, int extent, int mode) {
    if (mode == 0) return clamp(coordinate, 0, extent - 1);
    int period = mode == 2 ? extent * 2 : extent;
    int wrapped = ((coordinate % period) + period) % period;
    return mode == 2 && wrapped >= extent ? period - wrapped - 1 : wrapped;
}
float normalizedCoordinate(float value, int mode) {
    if (mode == 0) return saturate(value);
    if (mode == 1) return frac(value);
    float period = fmod(value, 2);
    if (period < 0) period += 2;
    return period > 1 ? 2 - period : period;
}
float4 texel(int2 pixel, int2 size) {
    pixel = int2(address(pixel.x,size.x,int(sampling.y)),address(pixel.y,size.y,int(sampling.z)));
    float4 color = baseColorImage.Load(int3(pixel,0));
    if(options.x>.5) color.rgb=color.a>0?saturate(color.rgb/color.a):0;
    if(options.y>.5) color.rgb=select(color.rgb<=.04045,color.rgb/12.92,pow((color.rgb+.055)/1.055,2.4));
    return float4(color.rgb * color.a, color.a);
}
float4 main(FragmentInput input) : SV_Target0 {
    uint width,height; baseColorImage.GetDimensions(width,height);
    int2 size=int2(width,height);
    float2 uv=float2(normalizedCoordinate(input.uv.x,int(sampling.y)),
                      normalizedCoordinate(input.uv.y,int(sampling.z)));
    float2 position=uv*size-.5;
    float4 color;
    if(sampling.x>.5) {
        int2 base=int2(floor(position)); float2 f=frac(position);
        color=lerp(lerp(texel(base,size),texel(base+int2(1,0),size),f.x),
                   lerp(texel(base+int2(0,1),size),texel(base+1,size),f.x),f.y);
    } else color=texel(int2(floor(position+.5)),size);
    color.rgb=color.a>0?color.rgb/color.a:0;
    color *= linearTint;
    if(options.z==1 && color.a<options.w) discard;
    if(options.z<2) color.a=1;
    return float4(color.rgb * color.a, color.a);
}
