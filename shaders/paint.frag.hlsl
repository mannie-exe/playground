Texture2D<float4> image : register(t0, space2);
SamplerState imageSampler : register(s0, space2);
struct Shape {
    float4 rowX;
    float4 rowY;
    float4 bounds;
    float4 top;
    float4 bottom;
};
struct PaintData {
    float4 tint;
    float4 borderTint;
    float4 sourceRect;
    // clip count, ring/box, sample image, reserved
    float4 options;
    // premultiplied input, sRGB input, linear filtering, atlas alpha-only
    float4 imageOptions;
    Shape outer;
    Shape inner;
    Shape clips[32];
};
struct DrawData { float4 bounds; PaintData paint; };
StructuredBuffer<DrawData> draws : register(t1, space2);
cbuffer Batch : register(b0, space3) { float4 batchOptions; };
cbuffer Path : register(b1, space3) {
    // segment count, even-odd fill, stroke radius, stroke-only
    float4 pathOptions;
    float4 segments[255];
};
bool pathContains(float2 p) {
    int winding = 0;
    for (int i = 0; i < int(pathOptions.x); ++i) {
        float2 a = segments[i].xy, b = segments[i].zw;
        float2 d = b - a;
        if (pathOptions.w > .5) {
            float len = dot(d,d);
            float t = len > 0 ? saturate(dot(p-a,d)/len) : 0;
            float2 delta = p-a-t*d;
            if (dot(delta,delta) <= pathOptions.z*pathOptions.z) return true;
        } else {
            float cross = d.x*(p.y-a.y) - (p.x-a.x)*d.y;
            if (a.y <= p.y && b.y > p.y && cross > 0) ++winding;
            if (a.y > p.y && b.y <= p.y && cross < 0) --winding;
        }
    }
    if (pathOptions.w > .5) return false;
    return pathOptions.y > .5 ? (winding % 2 != 0) : winding != 0;
}
float2 local(Shape s, float2 p) {
    return float2(dot(s.rowX.xy, p) + s.rowX.z, dot(s.rowY.xy, p) + s.rowY.z);
}
bool contains(Shape s, float2 p) {
    float2 q = local(s, p) - s.bounds.xy;
    float2 extent = s.bounds.zw;
    if (any(q < 0) || any(q >= extent)) return false;
    float2 radius = 0, center = 0;
    if (q.x < s.top.x && q.y < s.top.y) { radius=s.top.xy; center=radius; }
    else if(q.x > extent.x-s.top.z && q.y<s.top.w) { radius=s.top.zw; center=float2(extent.x-radius.x,radius.y); }
    else if(q.x>extent.x-s.bottom.x && q.y>extent.y-s.bottom.y) { radius=s.bottom.xy; center=extent-radius; }
    else if(q.x<s.bottom.z && q.y>extent.y-s.bottom.w) { radius=s.bottom.zw; center=float2(radius.x,extent.y-radius.y); }
    if (any(radius <= 0)) return true;
    float2 d = (q-center)/radius;
    return dot(d,d)<=1;
}
float3 decode(float3 c) { return select(c <= .04045, c / 12.92, pow((c+.055)/1.055, 2.4)); }
float3 encode(float3 c) { return select(c <= .0031308, c*12.92, 1.055*pow(max(c,0),1/2.4)-.055); }
float4 readPixel(int2 p, int2 size, float4 imageOptions) {
    float4 c = image.Load(int3(clamp(p,0,size-1),0));
    if (imageOptions.w > .5) c = float4(1,1,1,c.a);
    if (imageOptions.x > .5) c.rgb = c.a > 0 ? saturate(c.rgb / c.a) : 0;
    if (imageOptions.y > .5) c.rgb = decode(c.rgb);
    return float4(c.rgb*c.a,c.a);
}
float4 sampleImage(float2 uv, float4 imageOptions) {
    uint w,h; image.GetDimensions(w,h); int2 size=int2(w,h);
    float2 p=uv*size-.5;
    if (imageOptions.z < .5) return readPixel(int2(floor(p+.5)),size,imageOptions);
    int2 base=int2(floor(p)); float2 f=frac(p);
    return lerp(lerp(readPixel(base,size,imageOptions),readPixel(base+int2(1,0),size,imageOptions),f.x),
                lerp(readPixel(base+int2(0,1),size,imageOptions),readPixel(base+1,size,imageOptions),f.x),f.y);
}
float4 main(float4 position : SV_Position, nointerpolation uint drawIndex : TEXCOORD0) : SV_Target0 {
    PaintData paint = draws[drawIndex].paint;
    float outerCoverage=0, innerCoverage=0;
    [unroll] for (int y=0;y<4;++y) [unroll] for (int x=0;x<4;++x) {
        float2 p=position.xy + (float2(x,y)+.5)/4-.5;
        bool inside=contains(paint.outer,p);
        if (inside && pathOptions.x > 0) inside = pathContains(local(paint.outer,p));
        for(int c=0;c<int(paint.options.x);++c) inside=inside && contains(paint.clips[c],p);
        if(inside) { outerCoverage+=1.0/16; if(contains(paint.inner,p)) innerCoverage+=1.0/16; }
    }
    float4 color=float4(paint.tint.rgb*paint.tint.a,paint.tint.a);
    if(paint.options.z>.5) {
        float2 uv=paint.sourceRect.xy+(local(paint.outer,position.xy)-paint.outer.bounds.xy)/paint.outer.bounds.zw*paint.sourceRect.zw;
        float4 sampled=sampleImage(uv,paint.imageOptions);
        color=sampled*float4(paint.tint.rgb*paint.tint.a,paint.tint.a);
    }
    if(paint.options.y>.5) color=color*innerCoverage+float4(paint.borderTint.rgb*paint.borderTint.a,paint.borderTint.a)*(outerCoverage-innerCoverage);
    else color*=outerCoverage;
    if(batchOptions.x>.5) color.rgb=encode(color.a>0?color.rgb/color.a:0)*color.a;
    return color;
}
