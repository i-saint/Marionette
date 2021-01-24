Texture2D<float> g_image : register(t0);
Texture2D<float> g_template : register(t1);
RWTexture2D<float> g_result : register(u0);

#define CacheCapacity 8192
groupshared float s_template[CacheCapacity];

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    float r = 0.0f;

    uint w, h;
    g_template.GetDimensions(w, h);

    const uint cache_height = CacheCapacity / w;
    const uint cache_size = cache_height * w;
    for (uint i = 0; i < h; ++i) {
        uint cy = i % cache_height;
        if (cy == 0) {
            // wait previous loop
            GroupMemoryBarrierWithGroupSync();

            // preload template
            const uint read_block = 32;
            for (uint b = 0; b < read_block; ++b) {
                uint ci = gi * read_block + b;
                uint ti = w * i + ci;
                if (ci < cache_size)
                    s_template[ci] = g_template[uint2(ti % w, ti / w)];
            }
            GroupMemoryBarrierWithGroupSync();
        }

        for (uint j = 0; j < w; ++j) {
            float s = g_image[tid + uint2(j, i)];
            float t = s_template[w * cy + j];
            float diff = abs(s - t);
            r += diff;
        }
    }
    g_result[tid] = r / float(w * h);
}


#if 0
[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    float r = 0.0f;

    uint w, h;
    g_template.GetDimensions(w, h);
    for (uint i = 0; i < h; ++i) {
        for (uint j = 0; j < w; ++j) {
            uint2 pos = uint2(j, i);
            float s = g_image[tid + pos];
            float t = g_template[pos];
            float diff = abs(s - t);
            r += diff;
        }
    }
    g_result[tid] = r / float(w * h);
}
#endif
