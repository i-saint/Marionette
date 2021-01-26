Texture2D<uint> g_image : register(t0);
Texture2D<uint> g_template : register(t1);
RWTexture2D<uint> g_result : register(u0);


uint lshift(uint a, uint b, uint s)
{
    return s == 0 ? a : (a >> s) | (b << (32 - s));
}

#define CacheCapacity 8192
groupshared uint s_template[CacheCapacity];

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    uint w, h;
    g_template.GetDimensions(w, h);

    const uint pixel_shift = tid.x / 32;
    const uint bit_shift = tid.x % 32;
    const uint cache_height = CacheCapacity / w;
    const uint cache_size = cache_height * w;

    uint r = 0;
    for (uint i = 0; i < h; ++i) {
        uint cy = i % cache_height;
        if (cy == 0) {
            // preload template
            GroupMemoryBarrierWithGroupSync();
            for (uint b = 0; b < 32; ++b) {
                uint ci = gi * 32 + b;
                uint ti = w * i + ci;
                if (ci < cache_size)
                    s_template[ci] = g_template[uint2(ti % w, ti / w)];
            }
            GroupMemoryBarrierWithGroupSync();
        }

        uint py = tid.y + i;
        for (uint j = 0; j < w; ++j) {
            uint px = pixel_shift + j;
            uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
            uint tv = s_template[w * cy + j];
            r += countbits(iv & tv);
        }
    }
    g_result[tid] = r;
}

#if 0
[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_template.GetDimensions(w, h);

    const uint pixel_shift = tid.x / 32;
    const uint bit_shift = tid.x % 32;

    uint r = 0;
    for (uint i = 0; i < h; ++i) {
        uint py = tid.y + i;
        for (uint j = 0; j < w; ++j) {
            uint px = pixel_shift + j;
            uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
            uint tv = g_template[uint2(j, i)];
            r += countbits(iv & tv);
        }
    }
    g_result[tid] = r;
}
#endif
