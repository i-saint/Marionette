cbuffer Constants : register(b0)
{
    uint2 g_range;
    uint2 g_tl;
    uint2 g_br;
    uint2 g_template_size; // for TemplateMatch_Binary
};

Texture2D<float3> g_image : register(t0);
Texture2D<float3> g_template : register(t1);
Texture2D<float> g_mask_image : register(t2);
Texture2D<float> g_mask_template : register(t3);
RWTexture2D<float> g_result : register(u0);


float GetMaskValue(uint2 ipos, uint2 tpos)
{
    float r = 1.0f;

    uint2 imamge_size, imask_size;
    g_image.GetDimensions(imamge_size.x, imamge_size.y);
    g_mask_image.GetDimensions(imask_size.x, imask_size.y);
    if (imamge_size.x == imask_size.x && imamge_size.y == imask_size.y) {
        r *= g_mask_image[ipos];
    }

    uint2 template_size, tmask_size;
    g_template.GetDimensions(template_size.x, template_size.y);
    g_mask_template.GetDimensions(tmask_size.x, tmask_size.y);
    if (template_size.x == tmask_size.x && template_size.y == tmask_size.y) {
        r *= g_mask_template[tpos];
    }

    return r;
}

[numthreads(32, 32, 1)]
void main(uint2 tid : SV_DispatchThreadID)
{
    uint2 template_size, mask_size;
    g_template.GetDimensions(template_size.x, template_size.y);
    g_mask_template.GetDimensions(mask_size.x, mask_size.y);

    const uint2 bpos = g_tl + tid;
    const uint tw = template_size.x;
    const uint th = template_size.y;

    float r = 0.0f;
    for (uint i = 0; i < th; ++i) {
        for (uint j = 0; j < tw; ++j) {
            uint2 tpos = uint2(j, i);
            uint2 ipos = bpos + tpos;
            float3 s = g_image[ipos].xyz;
            float3 t = g_template[tpos].xyz;
            float3 diff = abs(s - t) * GetMaskValue(ipos, tpos);
            r += max(max(diff.x, diff.y), diff.z);
        }
    }

    if (tid.x < g_range.x && tid.y < g_range.y) {
        g_result[tid] = r;
    }
}
