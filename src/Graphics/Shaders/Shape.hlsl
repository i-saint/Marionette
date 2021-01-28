#define ST_Circle 1
#define ST_Rect 2

struct ShapeData
{
    int type;
    float border;
    int2 pos;
    float4 color;
    float radius;
    int2 rect_size;
    int pad;
};

StructuredBuffer<ShapeData> g_shapes : register(t0);
RWTexture2D<float4> g_image : register(u0);

void SetColor(uint2 tid, float4 color)
{
    float4 c = g_image[tid];
    c.rgb = lerp(c.rgb, color.rgb, color.a);
    g_image[tid] = c;
}

void Rect(uint2 tid, ShapeData shape)
{
    int2 pos = int2(tid) - shape.pos;
    int2 br = shape.rect_size;
    if (pos.x >= 0 && pos.x < br.x && pos.y >= 0 && pos.y < br.y) {
        int dx = min(pos.x, abs(pos.x - br.x));
        int dy = min(pos.y, abs(pos.y - br.y));
        if (min(dx, dy) < shape.border)
            SetColor(tid, shape.color);
    }
}

void Circle(uint2 tid, ShapeData shape)
{
    int2 pos = int2(tid) - shape.pos;
    float radius = shape.radius;
    float d = distance(float2(tid), float2(shape.pos));
    if (d < radius && radius - d < shape.border)
        SetColor(tid, shape.color);
}

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint n, s;
    g_shapes.GetDimensions(n, s);
    for (uint i = 0; i < n; ++i) {
        ShapeData s = g_shapes[i];
        if (s.type == ST_Rect)
            Rect(tid, s);
        else if (s.type == ST_Circle)
            Circle(tid, s);
        else
            break;
    }
}
