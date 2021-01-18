cbuffer Constants : register(b0)
{
    float2 g_pixel_size;
    int g_grayscale;
    int g_pad;
};

Texture2D<float4> g_src : register(t0);
RWTexture2D<float4> g_dst : register(u0);
SamplerState g_sampler : register(s0);

[numthreads(4, 4, 1)]
void main(uint3 tid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID, uint3 gid : SV_GroupID)
{
    float2 coord = g_pixel_size * (float2(tid.xy) + 0.5f);
    float4 p = g_src.SampleLevel(g_sampler, coord, 0);
    if (g_grayscale)
        g_dst[tid.xy] = dot(p.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    else
        g_dst[tid.xy] = p;
}
