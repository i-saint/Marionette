cbuffer Constants : register(b0)
{
    int g_block_size; // assume < 16
    int3 g_pad;
};

Texture2D<uint> g_image : register(t0);
RWTexture2D<uint> g_result : register(u0);


uint lshift(uint a, uint b, uint s)
{
    return s == 0 ? a : (a >> s) | (b << (32 - s));
}

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_image.GetDimensions(w, h);

    int _b = (g_block_size >> 1);
    int b_ = (g_block_size >> 1) + (g_block_size & 0x1);
    uint top = max(int(tid.y) - _b, 0);
    uint bottom = min(tid.y + b_, h);

    uint r = 0;
    for (uint b = 0; b < 32; ++b) {
        uint bx = tid.x * 32 + b;
        uint left = max(int(bx) - _b, 0);
        uint right = min(bx + b_, w * 32);

        uint px = left / 32;
        uint shift = left % 32;
        uint mask = (1 << (right - left)) - 1;

        uint bits = 0;
        for (uint py = top; py < bottom; ++py) {
            uint p = lshift(g_image[uint2(px, py)], g_image[uint2(px + 1, py)], shift);
            bits += countbits(p & mask);
        }
        if (bits)
            r |= 1 << b;
    }
    g_result[tid] = r;
}