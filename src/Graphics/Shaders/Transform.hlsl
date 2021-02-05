#define F_Grayscale 0x01
#define F_FillAlpha 0x02

cbuffer Constants : register(b0)
{
    float2 g_pixel_size;
    float2 g_pixel_offset;
    float2 g_sample_step;
    uint g_flags;
    uint g_filter;
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
    float2 s = g_sample_step / 3.0f;
    float2 t = s * 2.0f;
    float w = 1.0f / 4.0f;

    float4 r = 0.0f;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            float2 offset = (t * float2(x, y)) - s;
            r += g_src.SampleLevel(g_sampler, coord + offset, 0) * w;
        }
    }
    return r;
}

float4 Sample3x3(float2 coord)
{
    float2 s = g_sample_step / 3.0f;
    float2 t = s;
    float w = 1.0f / 9.0f;

    float4 r = 0.0f;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            float2 offset = (t * float2(x, y)) - s;
            r += g_src.SampleLevel(g_sampler, coord + offset, 0) * w;
        }
    }
    return r;
}

float4 Sample4x4(float2 coord)
{
    float2 s = g_sample_step / 3.0f;
    float2 t = s / 1.5f;
    float w = 1.0f / 16.0f;

    float4 r = 0.0f;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            float2 offset = (t * float2(x, y)) - s;
            r += g_src.SampleLevel(g_sampler, coord + offset, 0) * w;
        }
    }
    return r;
}

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    float2 coord = (g_sample_step * float2(tid)) + g_pixel_offset + (g_pixel_size * 0.5f);
    float4 p;
    switch (g_filter) {
    case 2: p = Sample2x2(coord); break;
    case 3: p = Sample3x3(coord); break;
    case 4: p = Sample4x4(coord); break;
    default: p = Sample1x1(coord); break;
    }

    if (g_flags & F_Grayscale) {
        g_dst[tid] = dot(p.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        //g_dst[tid] = min(min(p.r, p.g), p.b);
    }
    else {
        if (g_flags & F_FillAlpha)
            p.a = 1.0f;
        g_dst[tid] = p;
    }
}
