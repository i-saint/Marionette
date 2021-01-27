Texture2D<float> g_image : register(t0);
Texture2D<float> g_template : register(t1);
Texture2D<float> g_mask : register(t2);
RWTexture2D<float> g_result : register(u0);

#define EnableGroupShared

#ifdef EnableGroupShared

#define CacheCapacity 4096
groupshared float s_template[CacheCapacity];
groupshared float s_mask[CacheCapacity];

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    uint mw, mh;
    g_mask.GetDimensions(mw, mh);

    uint tw, th;
    g_template.GetDimensions(tw, th);

    const uint cache_height = CacheCapacity / tw;
    const uint cache_size = cache_height * tw;

    float r = 0.0f;
    if (mw != tw) {
        // without mask
        for (uint i = 0; i < th; ++i) {
            uint cy = i % cache_height;
            if (cy == 0) {
                // wait previous loop
                GroupMemoryBarrierWithGroupSync();

                // preload template
                const uint read_block = 32;
                for (uint b = 0; b < read_block; ++b) {
                    uint ci = gi * read_block + b;
                    uint ti1 = tw * i + ci;
                    if (ci < cache_size) {
                        uint2 ti = uint2(ti1 % tw, ti1 / tw);
                        s_template[ci] = g_template[ti];
                    }
                }
                GroupMemoryBarrierWithGroupSync();
            }

            for (uint j = 0; j < tw; ++j) {
                uint ci = tw * cy + j;
                float s = g_image[tid + uint2(j, i)];
                float t = s_template[ci];
                float diff = abs(s - t);
                r += diff;
            }
        }
    }
    else {
        // with mask
        for (uint i = 0; i < th; ++i) {
            uint cy = i % cache_height;
            if (cy == 0) {
                // wait previous loop
                GroupMemoryBarrierWithGroupSync();

                // preload template
                const uint read_block = 32;
                for (uint b = 0; b < read_block; ++b) {
                    uint ci = gi * read_block + b;
                    uint ti1 = tw * i + ci;
                    if (ci < cache_size) {
                        uint2 ti = uint2(ti1 % tw, ti1 / tw);
                        s_template[ci] = g_template[ti];
                        s_mask[ci] = g_mask[ti];
                    }
                }
                GroupMemoryBarrierWithGroupSync();
            }

            for (uint j = 0; j < tw; ++j) {
                uint ci = tw * cy + j;
                float s = g_image[tid + uint2(j, i)];
                float t = s_template[ci];
                float diff = abs(s - t) * s_mask[ci];
                r += diff;
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

    float r = 0.0f;
    if (mw != tw) {
        // without mask
        for (uint i = 0; i < th; ++i) {
            for (uint j = 0; j < tw; ++j) {
                uint2 pos = uint2(j, i);
                float s = g_image[tid + pos];
                float t = g_template[pos];
                float diff = abs(s - t);
                r += diff;
            }
        }
    }
    else {
        // with mask
        for (uint i = 0; i < th; ++i) {
            for (uint j = 0; j < tw; ++j) {
                uint2 pos = uint2(j, i);
                float s = g_image[tid + pos];
                float t = g_template[pos];
                float diff = abs(s - t) * g_mask[pos];
                r += diff;
            }
        }
    }

    g_result[tid] = r;
}

#endif // EnableGroupShared
