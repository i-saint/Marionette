cbuffer Constants : register(b0)
{
    float g_radius;
    float g_strength;
    int2 g_pad;
};

Texture2D<float> g_image : register(t0);
RWTexture2D<float> g_result : register(u0);

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_image.GetDimensions(w, h);

    int radius = ceil(g_radius);
    int2 ul = max(int2(tid) - radius, 0);
    int2 br = min(int2(tid) + radius + 1, int2(w, h));

    float cmin, cmax;
    cmin = cmax = g_image[tid];
    for (int i = ul.y; i < br.y; ++i) {
        for (int j = ul.x; j < br.x; ++j) {
            if (distance(float2(tid), float2(j, i)) <= g_radius) {
                float c = g_image[uint2(j, i)];
                cmin = min(c, cmin);
                cmax = max(c, cmax);
            }
        }
    }
    g_result[tid] = saturate((cmax - cmin) * g_strength);
}
