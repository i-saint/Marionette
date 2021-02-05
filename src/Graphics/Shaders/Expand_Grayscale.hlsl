cbuffer Constants : register(b0)
{
    float g_radius;
    int3 g_pad;
};

Texture2D<float> g_image : register(t0);
RWTexture2D<float> g_result : register(u0);

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_image.GetDimensions(w, h);

    int2 tl = max(int2(tid) - int(g_radius), 0);
    int2 br = min(int2(tid) + int(g_radius) + 1, int2(w, h));

    float r = g_image[tid];
    for (int i = tl.y; i < br.y; ++i)
        for (int j = tl.x; j < br.x; ++j)
            if (distance(float2(tid), float2(j, i)) <= g_radius)
                r = max(r, g_image[uint2(j, i)]);
    g_result[tid] = r;
}
