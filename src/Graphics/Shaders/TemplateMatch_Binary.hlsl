cbuffer Constants : register(b0)
{
    uint2 g_range;          // 
    uint2 g_tl;             // 
    uint2 g_br;             // 
    uint2 g_template_size;  // width is in bits
};

Texture2D<uint> g_image : register(t0);
Texture2D<uint> g_template : register(t1);
Texture2D<float> g_mask_image : register(t2);
Texture2D<float> g_mask_template : register(t3);
RWTexture2D<uint> g_result : register(u0);


uint lshift(uint a, uint b, uint s)
{
    return s == 0 ? a : (a >> s) | (b << (32 - s));
}

#define EnableGroupShared

#ifdef EnableGroupShared

#define CacheCapacity 4096
groupshared uint s_template[CacheCapacity];
groupshared uint s_mask[CacheCapacity];

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    // template_size.x is divided by 32 as the image is binary and the texture format is uint32.
    // g_template_size.x is actual width.

     uint2 template_size, mask_size;
    g_template.GetDimensions(template_size.x, template_size.y);
    g_mask_template.GetDimensions(mask_size.x, mask_size.y);

    const uint tw = template_size.x;
    const uint th = template_size.y;

    const uint px_offset = (g_tl.x + tid.x) / 32;
    const uint bit_shift = (g_tl.x + tid.x) % 32;
    const uint edge_mask = (1 << (g_template_size.x % 32)) - 1;
    const uint cache_height = CacheCapacity / tw;
    const uint cache_size = cache_height * tw;

    uint r = 0;
    if (template_size.x != mask_size.x) {
        // without mask
        for (uint i = 0; i < th; ++i) {
            uint cy = i % cache_height;
            if (cy == 0) {
                // preload template
                GroupMemoryBarrierWithGroupSync();
                for (uint b = 0; b < 32; ++b) {
                    uint ci = gi * 32 + b;
                    uint ti1 = tw * i + ci;
                    if (ci < cache_size) {
                        uint2 ti = uint2(ti1 % tw, ti1 / tw);
                        s_template[ci] = g_template[ti];
                    }
                }
                GroupMemoryBarrierWithGroupSync();
            }

            uint py = g_tl.y + tid.y + i;
            for (uint j = 0; j < tw; ++j) {
                uint px = px_offset + j;
                uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
                uint tv = s_template[tw * cy + j];
                uint bits = iv ^ tv;
                if (j == tw - 1)
                    bits &= edge_mask;
                r += countbits(bits);
            }
        }
    }
    else {
        // with mask
        for (uint i = 0; i < th; ++i) {
            uint cy = i % cache_height;
            if (cy == 0) {
                // preload template
                GroupMemoryBarrierWithGroupSync();
                for (uint b = 0; b < 32; ++b) {
                    uint ci = gi * 32 + b;
                    uint ti1 = tw * i + ci;
                    if (ci < cache_size) {
                        uint2 ti = uint2(ti1 % tw, ti1 / tw);
                        s_template[ci] = g_template[ti];
                        s_mask[ci] = g_mask_template[ti];
                    }
                }
                GroupMemoryBarrierWithGroupSync();
            }

            uint py = g_tl.y + tid.y + i;
            for (uint j = 0; j < tw; ++j) {
                uint px = px_offset + j;
                uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
                uint tv = s_template[tw * cy + j];
                uint bits = iv ^ tv;
                if (j == tw - 1)
                    bits &= edge_mask;
                uint mask = s_mask[tw * cy + j];
                r += countbits(bits & mask);
            }
        }
    }

    if (tid.x < g_range.x && tid.y < g_range.y) {
        g_result[tid] = r;
    }
}

#else // EnableGroupShared

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint2 template_size, mask_size;
    g_template.GetDimensions(template_size.x, template_size.y);
    g_mask_template.GetDimensions(mask_size.x, mask_size.y);

    const uint tw = template_size.x;
    const uint th = template_size.y;

    const uint px_offset = (g_tl.x + tid.x) / 32;
    const uint bit_shift = (g_tl.x + tid.x) % 32;
    const uint edge_mask = (1 << (g_template_size.x % 32)) - 1;

    uint r = 0;
    if (template_size.x != mask_size.x) {
        // without mask
        for (uint i = 0; i < th; ++i) {
            uint py = g_tl.y + tid.y + i;
            for (uint j = 0; j < tw; ++j) {
                uint px = px_offset + j;
                uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
                uint tv = g_template[uint2(j, i)];
                uint bits = iv ^ tv;
                if (j == tw - 1) {
                    bits &= edge_mask;
                }
                r += countbits(bits);
            }
        }
    }
    else {
        // with mask
        for (uint i = 0; i < th; ++i) {
            uint py = g_tl.y + tid.y + i;
            for (uint j = 0; j < tw; ++j) {
                uint px = px_offset + j;
                uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
                uint tv = g_template[uint2(j, i)];
                uint bits = iv ^ tv;
                if (j == tw - 1) {
                    bits &= edge_mask;
                }
                uint mask = g_mask_template[uint2(j, i)];
                r += countbits(bits & mask);
            }
        }
    }

    if (tid.x < g_range.x && tid.y < g_range.y) {
        g_result[tid] = r;
    }
}

#endif // EnableGroupShared
