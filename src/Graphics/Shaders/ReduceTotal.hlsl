#define BX 64

Texture2D<float> g_image : register(t0);
RWStructuredBuffer<float> g_result : register(u0);

groupshared float s_result[BX];


void ReduceGroup(uint gi)
{
    if (gi < 32) {
        s_result[gi] += s_result[gi + 32];
        s_result[gi] += s_result[gi + 16];
        s_result[gi] += s_result[gi + 8];
        s_result[gi] += s_result[gi + 4];
        s_result[gi] += s_result[gi + 2];
        s_result[gi] += s_result[gi + 1];
    }
}


// reduce horizontally
// assume Dispatch(1, height_of_g_image, 1)
[numthreads(BX, 1, 1)]
void Pass1(uint2 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    uint w, h;
    g_image.GetDimensions(w, h);

    float r = 0;
    for (uint x = tid.x + BX; x < w; x += BX) {
        r += g_image[uint2(x, tid.y)];
    }
    s_result[gi] = r;
    GroupMemoryBarrierWithGroupSync();

    ReduceGroup(gi);
    if (gi == 0) {
        g_result[tid.y] = s_result[0];
    }
}

// reduce vertically
// assume Dispatch(1, 1, 1)
[numthreads(BX, 1, 1)]
void Pass2(uint2 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    uint n, s;
    g_result.GetDimensions(n, s);

    uint bx = min(tid.x, n - 1);
    float r = g_result[bx];
    for (uint x = tid.x + BX; x < n; x += BX) {
        r += g_result[x];
    }
    s_result[gi] = r;
    GroupMemoryBarrierWithGroupSync();

    ReduceGroup(gi);
    if (gi == 0) {
        g_result[0] = s_result[0];
    }
}
