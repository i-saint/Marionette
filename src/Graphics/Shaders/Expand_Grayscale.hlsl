cbuffer Constants : register(b0)
{
    int g_size;
    int3 g_pad;
};

Texture2D<float> g_image : register(t0);
RWTexture2D<float> g_result : register(u0);

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_image.GetDimensions(w, h);

    int2 ul = int2(max(int2(tid) - g_size, 0));
    int2 br = int2(min(int2(tid) + g_size, int2(w, h)));

    float r = g_image[tid];
    for (int i = ul.y; i < br.y; ++i)
        for (int j = ul.x; j < br.x; ++j)
            r = max(r, g_image[int2(j, i)]);
    g_result[tid] = r;
}
