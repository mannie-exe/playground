#define PLAYGROUND_PAINT_LIBRARY
#include "paint.frag.hlsl"

// Only selected for axis-aligned rectangles without borders/rounded masks.
// inner.bounds stores the intersection in target pixel coordinates.
float4 main(float4 position : SV_Position,
            nointerpolation uint drawIndex : TEXCOORD0) : SV_Target0 {
  PaintData paint = draws[drawIndex].paint;
  float coverage = 1;
#ifndef PLAYGROUND_PRESENTATION
  float2 lo = max(position.xy - .5, paint.inner.bounds.xy);
  float2 hi = min(position.xy + .5, paint.inner.bounds.xy + paint.inner.bounds.zw);
  float2 overlap = saturate(hi - lo);
  coverage = overlap.x * overlap.y;
#endif
  float4 color = float4(paint.tint.rgb * paint.tint.a, paint.tint.a);
  if (paint.options.z > .5) {
    float2 uv = paint.sourceRect.xy +
                (local(paint.outer, position.xy) - paint.outer.bounds.xy) /
                    paint.outer.bounds.zw * paint.sourceRect.zw;
    float4 sampled;
    // Filtering is safe directly only in linear premultiplied storage.
    if (paint.imageOptions.x > .5 && paint.imageOptions.y < .5 &&
        paint.imageOptions.z > .5 && paint.imageOptions.w < .5)
      sampled = image.SampleLevel(imageSampler, uv, 0);
    else
      sampled = sampleImage(uv, paint.imageOptions);
    color *= sampled;
  }
  color *= coverage;
  if (batchOptions.x > .5)
    color.rgb = encode(color.a > 0 ? color.rgb / color.a : 0) * color.a;
  return color;
}
