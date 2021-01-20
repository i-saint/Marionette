cbuffer Constants : register(b0)
{
    float2 g_pixel_offset;
    float2 g_pixel_size;
    int g_grayscale;
    int3 g_pad;
};

Texture2D<float4> g_src : register(t0);
RWTexture2D<float4> g_dst : register(u0);
SamplerState g_sampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    float2 coord = g_pixel_size * (float2(tid) + 0.5f) + g_pixel_offset;
    float4 p = g_src.SampleLevel(g_sampler, coord, 0);
    if (g_grayscale)
        g_dst[tid] = dot(p.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    else
        g_dst[tid] = p;
}
