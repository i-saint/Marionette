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
    uint2 ur = uint2(max(tid - g_size, 0));
    uint2 bl = uint2(min(tid + g_size, uint2(w, h)));

    float cmin, cmax;
    cmin = cmax = g_image[tid];
    for (uint i = ur.y; i < bl.y; ++i) {
        for (uint j = ur.x; j < bl.x; ++j) {
            float c = g_image[uint2(j, i)];
            cmin = min(c, cmin);
            cmax = max(c, cmax);
        }
    }
    g_result[tid] = cmax - cmin;
}
