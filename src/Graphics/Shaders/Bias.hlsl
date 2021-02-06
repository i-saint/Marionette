cbuffer Constants : register(b0)
{
    float g_bias;
    float g_mul;
    int2 g_pad;
};

Texture2D<float> g_src : register(t0);
RWTexture2D<float> g_dst : register(u0);

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    g_dst[tid] = saturate((g_src[tid] - g_bias) * g_mul);
}
