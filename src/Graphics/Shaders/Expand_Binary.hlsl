cbuffer Constants : register(b0)
{
    int g_size;
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

    uint r = 0;
    // todo
    g_result[tid] = r;
}
