#include "pch.h"
#include "Filter.h"

// shader binaries
#include "Copy.hlsl.h"
#include "Contour.hlsl.h"
#include "MatchGrayscale.hlsl.h"
#include "ReduceMinMax_Pass1.hlsl.h"
#include "ReduceMinMax_Pass2.hlsl.h"

#define PassBin(A) g_hlsl_Copy, std::size(g_hlsl_Copy)

namespace mr {

Resize::Resize()
{
    m_ctx.initialize(PassBin(g_hlsl_Copy));
}

void Resize::setImage(Texture2DPtr v)
{
    if (m_src == v)
        return;
    m_src = v;
    m_ctx.setSRV(m_src);
    m_dirty = true;
}

void Resize::setResult(Texture2DPtr v)
{
    if (m_dst == v)
        return;
    m_dst = v;
    m_ctx.setUAV(m_dst);
    m_dirty = true;
}

void Resize::setCopyRegion(int2 pos, int2 size)
{
    if (pos == m_pos && size == m_size)
        return;
    m_pos = pos;
    m_size = size;
    m_dirty = true;
}

void Resize::setFlipRB(bool v)
{
    if (m_flip_rb == v)
        return;
    m_flip_rb = v;
    m_dirty = true;
}

void Resize::setGrayscale(bool v)
{
    if (m_grayscale == v)
        return;
    m_grayscale = v;
    m_dirty = true;
}

void Resize::dispatch()
{
    struct
    {
        float2 pixel_size;
        float2 pixel_offset;
        float2 sample_step;
        int flip_rb;
        int grayscale;
    } params;

    if (m_dirty) {
        //params.pixel_offset;
        int2 src_size = m_src->size();
        int2 dst_size = m_dst->size();
        if (m_size == int2::zero())
            m_size = dst_size;

        params.pixel_size = 1.0f / (float2)src_size;
        params.pixel_offset = params.pixel_size * m_pos;
        params.sample_step = (float2(m_size) / float2(src_size)) / float2(dst_size);
        params.flip_rb = m_flip_rb ? 1 : 0;
        params.grayscale = m_grayscale ? 1 : 0;

        m_const = Buffer::createConstant(&params, sizeof(params));
        m_ctx.setCBuffer(m_const);

        m_dirty = false;
    }

    m_ctx.dispatch(m_size.x, m_size.y);
}

void Resize::clear()
{
    m_ctx.clear();
}


Contour::Contour()
{
    m_ctx.initialize(PassBin(g_hlsl_Contour));
}


TemplateMatch::TemplateMatch()
{
    m_ctx_grayscale.initialize(PassBin(g_hlsl_MatchGrayscale));
    m_ctx_binary.initialize(PassBin(g_hlsl_MatchBinary));
}


ReduceMinMax::ReduceMinMax()
{
    m_ctx1.initialize(PassBin(g_hlsl_ReduceMinMax_Pass1));
    m_ctx2.initialize(PassBin(g_hlsl_ReduceMinMax_Pass2));
}

} // namespace mr
