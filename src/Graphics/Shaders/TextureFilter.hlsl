
float4 SampleTexture1x1(in Texture2D<float4> tex, in SamplerState smp, float2 uv)
{
    return tex.SampleLevel(smp, uv, 0);
}

float4 SampleTexture2x2(in Texture2D<float4> tex, in SamplerState smp, float2 uv, float2 block_size)
{
    float2 s = block_size / 3.0f;
    float2 t = s * 2.0f;
    float w = 1.0f / 4.0f;

    float4 r = 0.0f;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            float2 offset = (t * float2(x, y)) - s;
            r += tex.SampleLevel(smp, uv + offset, 0) * w;
        }
    }
    return r;
}

float4 SampleTexture3x3(in Texture2D<float4> tex, in SamplerState smp, float2 uv, float2 block_size)
{
    float2 s = block_size / 3.0f;
    float2 t = s;
    float w = 1.0f / 9.0f;

    float4 r = 0.0f;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            float2 offset = (t * float2(x, y)) - s;
            r += tex.SampleLevel(smp, uv + offset, 0) * w;
        }
    }
    return r;
}

float4 SampleTexture4x4(in Texture2D<float4> tex, in SamplerState smp, float2 uv, float2 block_size)
{
    float2 s = block_size / 3.0f;
    float2 t = s / 1.5f;
    float w = 1.0f / 16.0f;

    float4 r = 0.0f;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            float2 offset = (t * float2(x, y)) - s;
            r += tex.SampleLevel(smp, uv + offset, 0.0f) * w;
        }
    }
    return r;
}


static const float PI = 3.14159265359f;

float lanczos3(float x)
{
    if (x == 0.0f)
        return 1.0f;
    const float radius = 3.0f;
    float rx = x / radius;
    return (sin(PI * x) / (PI * x)) * (sin(PI * rx) / (PI * rx));
}

float4 SampleTextureLanczos3(in Texture2D<float4> tex, in SamplerState smp, float2 uv, float2 block_size)
{
    uint2 dim;
    tex.GetDimensions(dim.x, dim.y);
    float2 inv_dim = 1.0f / float2(dim);
    float2 pos = float2(dim) * uv;
    float2 f = frac(pos - 0.5f);

    float2 weights[6];
    for (int i = 0; i < 6; ++i) {
        float2 d = (float(i) - 2.5f) + f;
        weights[i] = float2(lanczos3(d.x), lanczos3(d.y));
        //weights[i] = 1.0f;
    }

    float4 r = 0.0f;
    float wt = 0.0f;
    for (int y = 0; y < 6; ++y) {
        float4 rx = 0.0f;
        for (int x = 0; x < 6; ++x) {
            float2 p = uv + ((float2(x, y) - 2.5f) / 5.0f * block_size);
            float w = weights[x].x + weights[y].y;
            r += tex.SampleLevel(smp, p, 0) * w;
            wt += w;
        }
    }
    return r / wt;
}

// The following code is licensed under the MIT license: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae
// Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
float4 SampleTextureCatmullRom(in Texture2D<float4> tex, in SamplerState smp, in float2 uv)
{
    uint2 tsi;
    tex.GetDimensions(tsi.x, tsi.y);
    float2 texSize = float2(tsi);

    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv * texSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1;
    float2 texPos3 = texPos1 + 2;
    float2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    float4 result = 0.0f;
    result += tex.SampleLevel(smp, float2(texPos0.x, texPos0.y), 0.0f) * w0.x * w0.y;
    result += tex.SampleLevel(smp, float2(texPos12.x, texPos0.y), 0.0f) * w12.x * w0.y;
    result += tex.SampleLevel(smp, float2(texPos3.x, texPos0.y), 0.0f) * w3.x * w0.y;

    result += tex.SampleLevel(smp, float2(texPos0.x, texPos12.y), 0.0f) * w0.x * w12.y;
    result += tex.SampleLevel(smp, float2(texPos12.x, texPos12.y), 0.0f) * w12.x * w12.y;
    result += tex.SampleLevel(smp, float2(texPos3.x, texPos12.y), 0.0f) * w3.x * w12.y;

    result += tex.SampleLevel(smp, float2(texPos0.x, texPos3.y), 0.0f) * w0.x * w3.y;
    result += tex.SampleLevel(smp, float2(texPos12.x, texPos3.y), 0.0f) * w12.x * w3.y;
    result += tex.SampleLevel(smp, float2(texPos3.x, texPos3.y), 0.0f) * w3.x * w3.y;

    return result;
}
