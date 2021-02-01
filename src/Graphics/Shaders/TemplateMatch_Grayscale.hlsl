cbuffer Constants : register(b0)
{
    uint2 g_range;
    uint2 g_tl;
    uint2 g_br;
    uint2 g_template_size;
};

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
    uint2 template_size, mask_size;
    g_template.GetDimensions(template_size.x, template_size.y);
    g_mask.GetDimensions(mask_size.x, mask_size.y);

    const uint2 result_size = g_range - template_size;
    const uint2 bpos = g_tl + tid;
    const uint tw = template_size.x;
    const uint th = template_size.y;

    const uint cache_height = CacheCapacity / tw;
    const uint cache_size = cache_height * tw;

    float r = 0.0f;
    if (template_size.x != mask_size.x) {
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
                float s = g_image[bpos + uint2(j, i)];
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
                float s = g_image[bpos + uint2(j, i)];
                float t = s_template[ci];
                float diff = abs(s - t) * s_mask[ci];
                r += diff;
            }
        }
    }

    if (tid.x < result_size.x && tid.y < result_size.y)
        g_result[tid] = r;
}

#else // EnableGroupShared

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint2 template_size, mask_size;
    g_template.GetDimensions(template_size.x, template_size.y);
    g_mask.GetDimensions(mask_size.x, mask_size.y);

    const uint2 result_size = g_range - template_size;
    const uint2 bpos = g_tl + tid;
    const uint tw = template_size.x;
    const uint th = template_size.y;

    float r = 0.0f;
    if (template_size.x != mask_size.x) {
        // without mask
        for (uint i = 0; i < th; ++i) {
            for (uint j = 0; j < tw; ++j) {
                uint2 pos = uint2(j, i);
                float s = g_image[bpos + pos];
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
                float s = g_image[bpos + pos];
                float t = g_template[pos];
                float diff = abs(s - t) * g_mask[pos];
                r += diff;
            }
        }
    }

    if (tid.x < result_size.x && tid.y < result_size.y)
        g_result[tid] = r;
}

#endif // EnableGroupShared
