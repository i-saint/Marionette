Texture2D<uint> g_image : register(t0);
Texture2D<uint> g_template : register(t1);
RWTexture2D<uint> g_result : register(u0);


uint lshift(uint a, uint b, uint s)
{
    return s == 0 ? a : (a >> s) | (b << (32 - s));
}

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_template.GetDimensions(w, h);

    uint r = 0;
    uint pixel_shift = tid.x / 32;
    uint bit_shift = tid.x % 32;
    for (uint i = 0; i < h; ++i) {
        uint py = tid.y + i;
        for (uint j = 0; j < w; ++j) {
            uint px = pixel_shift + j;
            uint iv = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], bit_shift);
            uint tv = g_template[uint2(j, i)];
            r += countbits(iv & tv);
        }
    }
    g_result[tid] = r;
}
