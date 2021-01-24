#include "pch.h"
#include "Filter.h"

// shader binaries
#include "Transform.hlsl.h"
#include "Contour.hlsl.h"
#include "Binarize.hlsl.h"
#include "TemplateMatch_Grayscale.hlsl.h"
#include "TemplateMatch_Binary.hlsl.h"
#include "ReduceMinMax_Pass1.hlsl.h"
#include "ReduceMinMax_Pass2.hlsl.h"

#define mrBytecode(A) A, std::size(A)

namespace mr {

IFilter::~IFilter()
{
}


Transform::Transform()
{
    m_ctx.initialize(mrBytecode(g_hlsl_Transform));
    m_ctx.setSampler(mrGfxDefaultSampler());
}

void Transform::setSrcImage(Texture2DPtr v)
{
    if (m_src == v)
        return;
    m_src = v;
    m_ctx.setSRV(m_src);
}

void Transform::setDstImage(Texture2DPtr v)
{
    if (m_dst == v)
        return;
    m_dst = v;
    m_ctx.setUAV(m_dst);
}

void Transform::setCopyRegion(int2 pos, int2 size)
{
    if (pos == m_pos && size == m_size)
        return;
    m_pos = pos;
    m_size = size;
    m_dirty = true;
}

void Transform::setFlipRB(bool v)
{
    if (m_flip_rb == v)
        return;
    m_flip_rb = v;
    m_dirty = true;
}

void Transform::setGrayscale(bool v)
{
    if (m_grayscale == v)
        return;
    m_grayscale = v;
    m_dirty = true;
}

void Transform::dispatch()
{
    if (!m_src || !m_dst)
        return;

    if (m_dirty) {
        int2 src_size = m_src->getSize();
        int2 dst_size = m_dst->getSize();
        if (m_size == int2::zero())
            m_size = src_size;

        struct
        {
            float2 pixel_size;
            float2 pixel_offset;
            float2 sample_step;
            int flip_rb;
            int grayscale;
        } params;
        params.pixel_size = 1.0f / float2(src_size);
        params.pixel_offset = params.pixel_size * m_pos;
        params.sample_step = (float2(m_size) / float2(src_size)) / float2(dst_size);
        params.flip_rb = m_flip_rb ? 1 : 0;
        params.grayscale = m_grayscale ? 1 : 0;

        m_const = Buffer::createConstant(params);
        m_ctx.setCBuffer(m_const);

        m_dirty = false;
    }

    m_ctx.dispatch(
        ceildiv(m_size.x, 32),
        ceildiv(m_size.y, 32));
}

void Transform::clear()
{
    m_ctx.clear();
    m_src = {};
    m_dst = {};
    m_const = {};
    m_dirty = true;
}


Contour::Contour()
{
    m_ctx.initialize(mrBytecode(g_hlsl_Contour));
}

void Contour::setSrcImage(Texture2DPtr v)
{
    if (m_src == v)
        return;
    m_src = v;
    m_ctx.setSRV(m_src);
}

void Contour::setDstImage(Texture2DPtr v)
{
    if (m_dst == v)
        return;
    m_dst = v;
    m_ctx.setUAV(m_dst);
}

void Contour::setBlockSize(int v)
{
    if (m_block_size == v)
        return;
    m_block_size = v;
    m_dirty = true;
}

void Contour::dispatch()
{
    if (!m_src || !m_dst)
        return;

    if (m_dirty) {
        struct
        {
            int range;
            int3 pad;
        } params{};
        params.range = (m_block_size - 1) / 2;

        m_const = Buffer::createConstant(params);
        m_ctx.setCBuffer(m_const);

        m_dirty = false;
    }

    auto size = m_dst->getSize();
    m_ctx.dispatch(
        ceildiv(size.x, 32),
        ceildiv(size.y, 32));
}

void Contour::clear()
{
    m_ctx.clear();
    m_src = {};
    m_dst = {};
    m_dirty = true;
}


Binarize::Binarize()
{
    m_ctx.initialize(mrBytecode(g_hlsl_Binarize));
}

void Binarize::setSrcImage(Texture2DPtr v)
{
    if (m_src == v)
        return;
    m_src = v;
    m_ctx.setSRV(m_src);
}

void Binarize::setDstImage(Texture2DPtr v)
{
    if (m_dst == v)
        return;
    m_dst = v;
    m_ctx.setUAV(m_dst);
}

void Binarize::setThreshold(float v)
{
    if (m_threshold == v)
        return;
    m_threshold = v;
    m_dirty = true;
}

void Binarize::dispatch()
{
    if (!m_src || !m_dst)
        return;

    if (m_dirty) {
        struct
        {
            float threshold;
            int3 pad;
        } params{};
        params.threshold = m_threshold;

        m_const = Buffer::createConstant(params);
        m_ctx.setCBuffer(m_const);

        m_dirty = false;
    }

    auto size = m_dst->getSize();
    m_ctx.dispatch(
        size.x,
        ceildiv(size.y, 32));
}

void Binarize::clear()
{
    m_ctx.clear();
    m_src = {};
    m_dst = {};
    m_dirty = true;
}


TemplateMatch::TemplateMatch()
{
    m_ctx_grayscale.initialize(mrBytecode(g_hlsl_TemplateMatch_Grayscale));
    m_ctx_binary.initialize(mrBytecode(g_hlsl_TemplateMatch_Binary));
}

void TemplateMatch::setSrcImage(Texture2DPtr v)
{
    if (m_src == v)
        return;
    m_src = v;
}

void TemplateMatch::setDstImage(Texture2DPtr v)
{
    if (m_dst == v)
        return;
    m_dst = v;
}

void TemplateMatch::setTemplateImage(Texture2DPtr v)
{
    if (m_template == v)
        return;
    m_template = v;
}

void TemplateMatch::dispatch()
{
    if (!m_src || !m_dst)
        return;

    auto size = m_dst->getSize();
    if (m_src->getFormat() == TextureFormat::Ru8) {
        m_ctx_grayscale.setSRV(m_src, 0);
        m_ctx_grayscale.setSRV(m_template, 1);
        m_ctx_grayscale.setUAV(m_dst);

        m_ctx_grayscale.dispatch(
            ceildiv(size.x, 32),
            ceildiv(size.y, 32));
    }
    else if (m_src->getFormat() == TextureFormat::Ri32) {
        m_ctx_binary.setSRV(m_src, 0);
        m_ctx_binary.setSRV(m_template, 1);
        m_ctx_binary.setUAV(m_dst);

        m_ctx_binary.dispatch(
            ceildiv(size.x, 32),
            ceildiv(size.y, 32));
    }
}

void TemplateMatch::clear()
{
    m_ctx_grayscale.clear();
    m_ctx_binary.clear();

    m_src = {};
    m_dst = {};
    m_template = {};
}


ReduceMinMax::ReduceMinMax()
{
    m_ctx1.initialize(mrBytecode(g_hlsl_ReduceMinMax_Pass1));
    m_ctx2.initialize(mrBytecode(g_hlsl_ReduceMinMax_Pass2));
}

void ReduceMinMax::setSrcImage(Texture2DPtr v)
{
    m_src = v;
}

void ReduceMinMax::dispatch()
{
    if (!m_src)
        return;

    struct Result
    {
        int2 pos_min;
        int2 pos_max;
        float val_min;
        float val_max;
        int pad[2];
    };
    mrCheck16(Result);

    size_t rsize = m_src->getSize().y * sizeof(Result);
    if (!m_dst || m_dst->getSize() != rsize)
        m_dst = Buffer::createStructured(rsize, sizeof(Result));
    if(!m_staging)
        m_staging = Buffer::createStaging(sizeof(Result));

    m_ctx1.setSRV(m_src);
    m_ctx1.setUAV(m_dst);
    m_ctx2.setSRV(m_src);
    m_ctx2.setUAV(m_dst);

    auto image_size = m_src->getSize();
    m_ctx1.dispatch(1, image_size.y);
    m_ctx2.dispatch(1, 1);
    DispatchCopy(m_staging, m_dst, sizeof(Result));

}

void ReduceMinMax::clear()
{
    m_src = {};
    m_dst = {};
    m_staging = {};

    m_ctx1.clear();
    m_ctx2.clear();
}

std::future<ReduceMinmaxResult> ReduceMinMax::getResult()
{
    return std::async(std::launch::deferred, [this]() {
        ReduceMinmaxResult ret{};
        MapRead(m_staging->ptr(), [&ret](const void* v) {
            ret = *(ReduceMinmaxResult*)v;
            });
        return ret;
        });
}

} // namespace mr
