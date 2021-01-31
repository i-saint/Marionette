#define BX 64
#include "Reduce_Common.hlsl"

Texture2D<value_type> g_image : register(t0);
RWStructuredBuffer<value_type> g_result : register(u0);

groupshared value_type s_result[BX];


void ReduceGroup(uint gi)
{
    GroupMemoryBarrierWithGroupSync();
    if (gi < 32)
        s_result[gi] += s_result[gi + 32];
    GroupMemoryBarrierWithGroupSync();
    if (gi < 16)
        s_result[gi] += s_result[gi + 16];
    GroupMemoryBarrierWithGroupSync();
    if (gi < 8)
        s_result[gi] += s_result[gi + 8];
    GroupMemoryBarrierWithGroupSync();
    if (gi < 4)
        s_result[gi] += s_result[gi + 4];
    GroupMemoryBarrierWithGroupSync();
    if (gi < 2)
        s_result[gi] += s_result[gi + 2];
    GroupMemoryBarrierWithGroupSync();
    if (gi < 1)
        s_result[gi] += s_result[gi + 1];
    GroupMemoryBarrierWithGroupSync();
}


// reduce horizontally
// assume Dispatch(1, height_of_g_image, 1)
[numthreads(BX, 1, 1)]
void Pass1(uint2 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    value_type r = 0;
    for (uint x = tid.x; x < g_range.x; x += BX)
        r += g_image[uint2(x, tid.y)];
    s_result[gi] = r;

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
    value_type r = 0;
    for (uint x = tid.x; x < g_range.y; x += BX)
        r += g_result[x];
    s_result[gi] = r;

    ReduceGroup(gi);
    if (gi == 0) {
        g_result[0] = s_result[0];
    }
}
