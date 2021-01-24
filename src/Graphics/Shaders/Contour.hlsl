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
    int2 ur = int2(max((int2)tid - g_size, 0));
    int2 bl = int2(min((int2)tid + g_size, int2(w, h)));

    float cmin, cmax;
    cmin = cmax = g_image[tid];
    for (int i = ur.y; i < bl.y; ++i) {
        for (int j = ur.x; j < bl.x; ++j) {
            float c = g_image[int2(j, i)];
            cmin = min(c, cmin);
            cmax = max(c, cmax);
        }
    }
    g_result[tid] = cmax - cmin;
}
