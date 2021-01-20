Texture2D<float> g_image : register(t0);
Texture2D<float> g_template : register(t1);
RWTexture2D<float> g_result : register(u0);

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    float r = 0.0f;

    uint w, h;
    g_template.GetDimensions(w, h);
    for (uint i = 0; i < h; ++i) {
        for (uint j = 0; j < w; ++j) {
            uint2 pos = uint2(j, i);
            float diff = abs(g_image[tid + pos] - g_template[pos]);
            r += diff * diff;
        }
    }
    g_result[tid] = r / float(w * h);
}
