Texture2D<uint> g_image : register(t0);
Texture2D<uint> g_template : register(t1);
RWTexture2D<float> g_result : register(u0);


uint shift(uint a, uint b, uint s)
{
    return (a >> s) | (b << (32 - s));
}

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_template.GetDimensions(w, h);

    uint r = 0;
    uint y = tid.y;
    for (uint i = 0; i < h; ++i) {
        for (uint j = 0; j < w; j += 32) {
            uint ibx = tid.x + j;
            uint ix = ibx / 32;
            uint iv = shift(g_image[uint2(ix, y)], g_image[uint2(ix + 1, y)], ibx % 32);
            uint tv = g_template[uint2(j / 32, y)];
            r += countbits(iv & tv);
        }
    }
    g_result[tid] = float(r) / 128.0f;
}
