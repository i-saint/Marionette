cbuffer Constants : register(b0)
{
    float g_rmax;
    int3 g_pad;
};

Texture2D<uint> g_src : register(t0);
RWTexture2D<float> g_dst : register(u0);

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    g_dst[tid] = float(g_src[tid]) * g_rmax;
}
