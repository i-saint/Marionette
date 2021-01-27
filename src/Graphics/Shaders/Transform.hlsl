cbuffer Constants : register(b0)
{
    float2 g_pixel_size;
    float2 g_pixel_offset;
    float2 g_sample_step;
    int g_grayscale;
    int g_filtering;
};

Texture2D<float4> g_src : register(t0);
RWTexture2D<float4> g_dst : register(u0);
SamplerState g_sampler : register(s0);

float4 Sample1x1(float2 coord)
{
    return g_src.SampleLevel(g_sampler, coord, 0);
}

float4 Sample2x2(float2 coord)
{
    float2 s = g_sample_step * 0.333333f;
    float w = 1.0f / 4.0f;

    float4 r = 0.0f;
    r += g_src.SampleLevel(g_sampler, coord + float2(-s.x, -s.y), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2( s.x, -s.y), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2(-s.x,  s.y), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2( s.x,  s.y), 0) * w;
    return r;
}

float4 Sample3x3(float2 coord)
{
    float2 s = g_sample_step * 0.333333f;
    float w = 1.0f / 9.0f;

    float4 r = 0.0f;
    r += g_src.SampleLevel(g_sampler, coord + float2(-s.x, -s.y), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2(0.0f, -s.y), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2( s.x, -s.y), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2(-s.x, 0.0f), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord, 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2( s.x, 0.0f), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2(-s.x,  s.y), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2(0.0f,  s.y), 0) * w;
    r += g_src.SampleLevel(g_sampler, coord + float2( s.x,  s.y), 0) * w;
    return r;
}

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    float2 coord = (g_sample_step * float2(tid)) + g_pixel_offset + (g_pixel_size * 0.5f);
    float4 p = g_filtering ? Sample2x2(coord) : Sample1x1(coord);

    if (g_grayscale)
        g_dst[tid] = dot(p.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    else
        g_dst[tid] = p;
}
