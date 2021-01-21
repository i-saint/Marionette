cbuffer Constants : register(b0)
{
    float2 g_pixel_size;
    float2 g_pixel_offset;
    float2 g_sample_step;
    int g_flip_rb;
    int g_grayscale;
};

Texture2D<float4> g_src : register(t0);
RWTexture2D<float4> g_dst : register(u0);
SamplerState g_sampler : register(s0);

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    float2 coord = (g_sample_step * float2(tid)) + g_pixel_offset + (g_pixel_size * 0.5f);
    float4 p = g_src.SampleLevel(g_sampler, coord, 0);

    if (g_flip_rb)
        p.rb = p.br;

    if (g_grayscale)
        g_dst[tid] = dot(p.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    else
        g_dst[tid] = p;
}
