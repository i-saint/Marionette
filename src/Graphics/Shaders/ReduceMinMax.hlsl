#define BX 64
#include "Reduce_Common.hlsl"

struct Result
{
    uint2 pmin, pmax;
    value_type vmin, vmax;
    int2 pad;
};

Texture2D<value_type> g_image : register(t0);
RWStructuredBuffer<Result> g_result : register(u0);

groupshared Result s_result[BX];


void Reduce(inout Result r, uint2 p, value_type v)
{
    if (v < r.vmin) {
        r.vmin = v;
        r.pmin = p;
    }
    if (v > r.vmax) {
        r.vmax = v;
        r.pmax = p;
    }
}

Result Reduce(Result a, Result b)
{
    Result r = a;
    if (b.vmin < r.vmin || (b.vmin == r.vmin && b.pmin.y < r.pmin.y)) {
        r.vmin = b.vmin;
        r.pmin = b.pmin;
    }
    if (b.vmax > r.vmax || (b.vmax == r.vmax && b.pmax.y < r.pmax.y)) {
        r.vmax = b.vmax;
        r.pmax = b.pmax;
    }
    return r;
}

void ReduceGroup(uint gi)
{
    GroupMemoryBarrierWithGroupSync();
    if (gi < 32)
        s_result[gi] = Reduce(s_result[gi], s_result[gi + 32]);
    GroupMemoryBarrierWithGroupSync();
    if (gi < 16)
        s_result[gi] = Reduce(s_result[gi], s_result[gi + 16]);
    GroupMemoryBarrierWithGroupSync();
    if (gi < 8)
        s_result[gi] = Reduce(s_result[gi], s_result[gi + 8]);
    GroupMemoryBarrierWithGroupSync();
    if (gi < 4)
        s_result[gi] = Reduce(s_result[gi], s_result[gi + 4]);
    GroupMemoryBarrierWithGroupSync();
    if (gi < 2)
        s_result[gi] = Reduce(s_result[gi], s_result[gi + 2]);
    GroupMemoryBarrierWithGroupSync();
    if (gi < 1)
        s_result[gi] = Reduce(s_result[gi], s_result[gi + 1]);
    GroupMemoryBarrierWithGroupSync();
}


// reduce horizontally
// assume Dispatch(1, height_of_g_image, 1)
[numthreads(BX, 1, 1)]
void Pass1(uint2 tid : SV_DispatchThreadID, uint gi : SV_GroupIndex)
{
    uint2 bpos = min(tid, g_range - 1);
    Result r;
    r.pmin = r.pmax = bpos;
    r.vmin = r.vmax = g_image[bpos];
    r.pad = 0;

    for (uint x = tid.x + BX; x < g_range.x; x += BX) {
        uint2 p = uint2(x, tid.y);
        Reduce(r, p, g_image[p]);
    }
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
    uint bx = min(tid.x, g_range.y - 1);
    Result r = g_result[bx];
    for (uint x = tid.x + BX; x < g_range.y; x += BX)
        r = Reduce(r, g_result[x]);
    s_result[gi] = r;

    ReduceGroup(gi);
    if (gi == 0) {
        g_result[0] = s_result[0];
    }
}
