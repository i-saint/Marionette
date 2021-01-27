Texture2D<uint> g_image : register(t0);
Texture2D<uint> g_template : register(t1);
Texture2D<uint> g_mask : register(t2);
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
    uint mw, mh;
    g_mask.GetDimensions(mw, mh);

    uint tw, th;
    g_template.GetDimensions(tw, th);

    const uint pixel_shift = tid.x / 32;
    const uint bit_shift = tid.x % 32;
    const uint cache_height = CacheCapacity / tw;
    const uint cache_size = cache_height * tw;

    uint r = 0;
    if (mw != tw) {
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

            uint py = tid.y + i;
            for (uint j = 0; j < tw; ++j) {
                uint px = pixel_shift + j;
                uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
                uint tv = s_template[tw * cy + j];
                r += countbits(iv ^ tv);
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
                        s_mask[ci] = g_mask[ti];
                    }
                }
                GroupMemoryBarrierWithGroupSync();
            }

            uint py = tid.y + i;
            for (uint j = 0; j < tw; ++j) {
                uint px = pixel_shift + j;
                uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
                uint tv = s_template[tw * cy + j];
                uint mask = s_mask[tw * cy + j];
                r += countbits((iv ^ tv) & mask);
            }
        }
    }
    g_result[tid] = r;
}

#else // EnableGroupShared

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint mw, mh;
    g_mask.GetDimensions(mw, mh);

    uint tw, th;
    g_template.GetDimensions(tw, th);

    const uint pixel_shift = tid.x / 32;
    const uint bit_shift = tid.x % 32;

    uint r = 0;
    if (mw != tw) {
        // without mask
        for (uint i = 0; i < th; ++i) {
            uint py = tid.y + i;
            for (uint j = 0; j < tw; ++j) {
                uint px = pixel_shift + j;
                uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
                uint tv = g_template[uint2(j, i)];
                r += countbits(iv ^ tv);
            }
        }
    }
    else {
        // with mask
        for (uint i = 0; i < th; ++i) {
            uint py = tid.y + i;
            for (uint j = 0; j < tw; ++j) {
                uint px = pixel_shift + j;
                uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
                uint tv = g_template[uint2(j, i)];
                uint mask = g_mask[uint2(j, i)];
                r += countbits((iv ^ tv) & mask);
            }
        }
    }
    g_result[tid] = r;
}

#endif // EnableGroupShared
