#include "TextureFilter.hlsl"

#define F_Grayscale 0x01
#define F_FillAlpha 0x02

cbuffer Constants : register(b0)
{
    float2 g_pixel_size;
    float2 g_pixel_offset;
    float2 g_sample_step;
    float2 g_bias;
    uint g_flags;
    uint g_filter;
    uint2 g_pad;
};

Texture2D<float4> g_src : register(t0);
RWTexture2D<float4> g_dst : register(u0);
SamplerState g_sampler_point : register(s0);
SamplerState g_sampler_linear : register(s1);



[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    float2 uv = (g_sample_step * float2(tid)) + g_pixel_offset + (g_sample_step * 0.5f);
    float4 p;
    switch (g_filter) {
    case 0: p = SampleTexture1x1(g_src, g_sampler_linear, uv); break;
    //case 1: p = SampleTextureCatmullRom(g_src, g_sampler_linear, uv); break;
    //case 2: p = SampleTexture2x2(g_src, g_sampler_linear, uv, g_sample_step); break;
    //case 3: p = SampleTexture3x3(g_src, g_sampler_linear, uv, g_sample_step); break;
    //case 4: p = SampleTexture4x4(g_src, g_sampler_linear, uv, g_sample_step); break;
    default: p = SampleTextureCatmullRom(g_src, g_sampler_linear, uv); break;
    //default: p = SampleTextureLanczos3(g_src, g_sampler_linear, uv, g_sample_step); break;
    //default: p = SampleTexture1x1(g_src, g_sampler_linear, uv); break;
    }

    if (g_flags & F_Grayscale) {
        float c = dot(p.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        g_dst[tid] = saturate((c - g_bias.x) * g_bias.y);
    }
    else {
        if (g_flags & F_FillAlpha)
            p.a = 1.0f;
        g_dst[tid] = saturate((p - g_bias.x) * g_bias.y);
    }
}
