cbuffer Constants : register(b0)
{
    uint g_block_size;
    int3 g_pad;
};

Texture2D<float> g_image : register(t0);
RWTexture2D<float> g_result : register(u0);

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_image.GetDimensions(w, h);

    int _b = (g_block_size >> 1);
    int b_ = (g_block_size >> 1) + (g_block_size & 0x1);
    int2 ul = max(int2(tid) - _b, 0);
    int2 br = min(int2(tid) + b_, int2(w, h));

    float r = g_image[tid];
    for (int i = ul.y; i < br.y; ++i)
        for (int j = ul.x; j < br.x; ++j)
            r = max(r, g_image[uint2(j, i)]);
    g_result[tid] = r;
}
