cbuffer Constants : register(b0)
{
    float g_threshold;
    int3 g_pad;
};

Texture2D<float> g_image : register(t0);
RWTexture2D<uint> g_result : register(u0);

[numthreads(1, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_image.GetDimensions(w, h);

    uint r = 0;
    uint2 base = uint2(tid.x * 32, tid.y);
    for (uint i = 0; i < 32; ++i) {
        uint2 pos = base + uint2(i, 0);
        if (pos.x < w) {
            float v = g_image[pos];
            if (v > g_threshold)
                r |= (1 << i);
        }
    }
    g_result[tid] = r;
}
