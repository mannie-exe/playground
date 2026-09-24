# Local fixes against SDL_ttf a1ce367 (3.2.2). Original zlib notices remain intact.
# Called after CPM population and before compilation. Exact-snippet guards reject
# upstream drift; repeated Debug/Release configuration is harmless.
function(_playground_ttf_replace source old replacement)
    file(READ "${source}" content)
    string(FIND "${content}" "${replacement}" patched)
    if(NOT patched EQUAL -1)
        return()
    endif()
    string(FIND "${content}" "${old}" original)
    if(original EQUAL -1)
        message(FATAL_ERROR "SDL_ttf text patch no longer matches ${source}; review dependency revision")
    endif()
    string(REPLACE "${old}" "${replacement}" content "${content}")
    file(WRITE "${source}" "${content}")
endfunction()

function(playground_patch_sdl_ttf source_dir)
    _playground_ttf_replace("${source_dir}/src/SDL_ttf.c"
[====[static void BG_Blended_Color(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, Uint8 fg_alpha)
{
    const Uint32 *src   = (Uint32 *)image->buffer;
    Uint32      *dst    = destination;
    Uint32       width  = image->width;
    Uint32       height = image->rows;

    if (fg_alpha == SDL_ALPHA_OPAQUE) {
        while (height--) {
            /* *INDENT-OFF* */
            DUFFS_LOOP4(
                *dst++ = *src++;
            , width);
            /* *INDENT-ON* */
            src = (const Uint32 *)((const Uint8 *)src + srcskip);
            dst = (Uint32 *)((Uint8 *)dst + dstskip);
        }
    } else {
        Uint32 alpha;
        Uint32 tmp;

        while (height--) {
            /* *INDENT-OFF* */
            DUFFS_LOOP4(
                    /* prevent misaligned load: tmp = *src++; */
                    // eventually, we can expect the compiler to replace the memcpy call with something optimized
                    SDL_memcpy(&tmp, src++, sizeof(tmp));
                    alpha = tmp >> 24;
                    tmp &= ~0xFF000000;
                    alpha = fg_alpha * alpha;
                    alpha =  DIVIDE_BY_255(alpha) << 24;
                    *dst++ = tmp | alpha
                    , width);
            /* *INDENT-ON* */
            src = (const Uint32 *)((const Uint8 *)src + srcskip);
            dst = (Uint32 *)((Uint8 *)dst + dstskip);
        }
    }
}

]====]
[====[static void BG_Blended_Color(const TTF_Image *image, Uint32 *destination, Sint32 srcskip, Uint32 dstskip, Uint8 fg_alpha)
{
    /* playground patch: FreeType color glyphs are encoded-premultiplied, but
       blended SDL text surfaces (including monochrome runs) use straight RGBA. */
    const Uint32 *src = (const Uint32 *)image->buffer;
    Uint32 *dst = destination;
    Uint32 height = image->rows;
    while (height--) {
        for (Uint32 x = 0; x < image->width; ++x) {
            Uint32 pixel;
            SDL_memcpy(&pixel, src++, sizeof(pixel));
            const Uint32 alpha = pixel >> 24;
            Uint32 straight = 0;
            if (alpha) {
                for (int shift = 0; shift < 24; shift += 8) {
                    const Uint32 channel = (pixel >> shift) & 255;
                    straight |= SDL_min(255u, (channel * 255u + alpha / 2u) / alpha) << shift;
                }
            }
            *dst++ = straight | (DIVIDE_BY_255(alpha * fg_alpha) << 24);
        }
        src = (const Uint32 *)((const Uint8 *)src + srcskip);
        dst = (Uint32 *)((Uint8 *)dst + dstskip);
    }
}

]====])
    _playground_ttf_replace("${source_dir}/src/SDL_ttf.c"
[====[        int glyph_width = glyph->sz_width;
        int glyph_rows = glyph->sz_rows;
        TTF_DrawOperation *op;

        // Position updated after glyph rendering
        x = xstart + FT_FLOOR(x) + glyph->sz_left;
        y = ystart + FT_FLOOR(y) - glyph->sz_top;

]====]
[====[        int glyph_width = glyph->sz_width;
        int glyph_rows = glyph->sz_rows;
        int glyph_left = glyph->sz_left;
        int glyph_top = glyph->sz_top;
        TTF_DrawOperation *op;

        /* playground patch: actual bitmap metrics include synthetic styles,
           outline expansion and layer-only COLR glyphs with empty base outlines. */
        if (!glyph_font->render_sdf) {
            TTF_Image *image;
            if (!Find_GlyphByIndex(glyph_font, idx, COLOR, 0, 0, NULL, &image)) {
                return false;
            }
            glyph_width = image->width;
            glyph_rows = image->rows;
            glyph_left = image->left;
            glyph_top = image->top;
        }

        x = xstart + FT_FLOOR(x) + glyph_left;
        y = ystart + FT_FLOOR(y) - glyph_top;

]====])
    _playground_ttf_replace("${source_dir}/src/SDL_ttf.c"
[====[            op->copy.src.w = glyph_width + 2 * font->outline;
            op->copy.src.h = glyph_rows + 2 * font->outline;]====]
[====[            op->copy.src.w = glyph_width + (glyph_font->render_sdf ? 2 * font->outline : 0);
            op->copy.src.h = glyph_rows + (glyph_font->render_sdf ? 2 * font->outline : 0);]====])
    _playground_ttf_replace("${source_dir}/src/SDL_gpu_textengine.c"
[====[            glyph = (AtlasGlyph *)ops[i].copy.reserved;
            SDL_memcpy(uv, glyph->texcoords, sizeof(glyph->texcoords));
            uv += SDL_arraysize(glyph->texcoords);]====]
[====[            glyph = (AtlasGlyph *)ops[i].copy.reserved;
            /* playground patch: preserve source clipping instead of stretching
               the whole glyph bitmap into a clipped destination rectangle. */
            const SDL_Rect *src = &ops[i].copy.src;
            const float du = (glyph->texcoords[2] - glyph->texcoords[0]) / glyph->rect.w;
            const float dv = (glyph->texcoords[5] - glyph->texcoords[1]) / glyph->rect.h;
            const float left = glyph->texcoords[0] + src->x * du;
            const float top = glyph->texcoords[1] + src->y * dv;
            const float right = left + src->w * du;
            const float bottom = top + src->h * dv;
            uv[0] = left; uv[1] = top;
            uv[2] = right; uv[3] = top;
            uv[4] = right; uv[5] = bottom;
            uv[6] = left; uv[7] = bottom;
            uv += SDL_arraysize(glyph->texcoords);]====])
endfunction()
