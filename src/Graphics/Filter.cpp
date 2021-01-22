#include "pch.h"
#include "Filter.h"

// shader binaries
#include "Transform.hlsl.h"
#include "Contour.hlsl.h"
#include "MatchGrayscale.hlsl.h"
#include "MatchBinary.hlsl.h"
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
    m_ctx.setSampler(mrGetDefaultSampler());
}

void Transform::setSrcImage(Texture2DPtr v)
{
    if (m_src == v)
        return;
    m_src = v;
    m_ctx.setSRV(m_src);
    m_dirty = true;
}

void Transform::setDstImage(Texture2DPtr v)
{
    if (m_dst == v)
        return;
    m_dst = v;
    m_ctx.setUAV(m_dst);
    m_dirty = true;
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
        int2 src_size = m_src->size();
        int2 dst_size = m_dst->size();
        if (m_size == int2::zero())
            m_size = dst_size;

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

    m_ctx.dispatch(m_size.x, m_size.y);
}

void Transform::clear()
{
    m_src = {};
    m_dst = {};
    m_const = {};
    m_ctx.clear();
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
    m_dirty = true;
}

void Contour::setDstImage(Texture2DPtr v)
{
    if (m_dst == v)
        return;
    m_dst = v;
    m_dirty = true;
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
    // todo
}

void Contour::clear()
{
    // todo
}


TemplateMatch::TemplateMatch()
{
    m_ctx_grayscale.initialize(mrBytecode(g_hlsl_MatchGrayscale));
    m_ctx_binary.initialize(mrBytecode(g_hlsl_MatchBinary));
}

void TemplateMatch::setImage(Texture2DPtr v)
{
    if (m_image == v)
        return;
    m_image = v;
    m_dirty = true;
}

void TemplateMatch::setTemplate(Texture2DPtr v)
{
    if (m_template == v)
        return;
    m_template = v;
    m_dirty = true;
}

void TemplateMatch::dispatch()
{
    // todo
}

void TemplateMatch::clear()
{
    // todo
}


ReduceMinMax::ReduceMinMax()
{
    mrCheck16(Result);
    m_ctx1.initialize(mrBytecode(g_hlsl_ReduceMinMax_Pass1));
    m_ctx2.initialize(mrBytecode(g_hlsl_ReduceMinMax_Pass2));
    m_staging = Buffer::createStaging(sizeof(Result));
}

void ReduceMinMax::setImage(Texture2DPtr v)
{
    m_src = v;

    if (v) {
        size_t rsize = v->size().y * sizeof(Result);
        if (!m_result || m_result->size() != rsize) {
            m_result = Buffer::createStructured(rsize, sizeof(Result));
        }

        m_ctx1.setSRV(v);
        m_ctx1.setUAV(m_result);
        m_ctx2.setSRV(v);
        m_ctx2.setUAV(m_result);
    }
}

void ReduceMinMax::dispatch()
{
    if (!m_src)
        return;

    auto image_size = m_src->size();
    m_ctx1.dispatch(1, image_size.y);
    m_ctx2.dispatch(1, 1);
    DispatchCopy(m_staging, m_result, sizeof(Result));

    auto fv = DeviceManager::get()->addFenceEvent();
    m_task = std::async(std::launch::async, [this, fv]() {
        DeviceManager::get()->waitFence(fv);
        Result ret{};
        MapRead(m_staging, [&ret](const void* v) {
            ret = *(Result*)v;
            });
        return ret;
        });
}

void ReduceMinMax::clear()
{
    m_ctx1.clear();
    m_ctx2.clear();
}

ReduceMinMax::Result ReduceMinMax::getResult()
{
    if (m_task.valid())
        return m_task.get();
    return {};
}

} // namespace mr
