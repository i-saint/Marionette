#include "pch.h"
#include "Filter.h"

// shader binaries
#include "Transform.hlsl.h"
#include "Contour.hlsl.h"
#include "Binarize.hlsl.h"
#include "TemplateMatch_Grayscale.hlsl.h"
#include "TemplateMatch_Binary.hlsl.h"

#include "ReduceTotal_FPass1.hlsl.h"
#include "ReduceTotal_FPass2.hlsl.h"
#include "ReduceTotal_IPass1.hlsl.h"
#include "ReduceTotal_IPass2.hlsl.h"

#include "ReduceCountBits_Pass1.hlsl.h"
#include "ReduceCountBits_Pass2.hlsl.h"

#include "ReduceMinMax_FPass1.hlsl.h"
#include "ReduceMinMax_FPass2.hlsl.h"
#include "ReduceMinMax_IPass1.hlsl.h"
#include "ReduceMinMax_IPass2.hlsl.h"

#define mrBytecode(A) A, std::size(A)

#define mrCheckDirty(...)\
    if (__VA_ARGS__) { return; }\
    m_dirty = true;


namespace mr {

ICompute::~ICompute()
{
}


TransformCS::TransformCS()
{
    m_cs.initialize(mrBytecode(g_hlsl_Transform));
    m_cs.setSampler(mrGfxDefaultSampler());
}

void TransformCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<Transform&>(ctx_);
    m_cs.setSRV(ctx.m_src);
    m_cs.setUAV(ctx.m_dst);
    m_cs.setCBuffer(ctx.m_const);
    m_cs.dispatch(
        ceildiv(ctx.m_size.x, 32),
        ceildiv(ctx.m_size.y, 32));
}

TransformPtr TransformCS::createContext()
{
    return make_ref<Transform>(this);
}

Transform::Transform(TransformCS* v)
    : m_cs(v)
{
}

void Transform::setSrc(ITexture2DPtr v)
{
    m_src = cast(v);
}

void Transform::setDst(ITexture2DPtr v)
{
    m_dst = cast(v);
}

void Transform::setRect(int2 offset, int2 size)
{
    mrCheckDirty(offset == m_offset && size == m_size);
    m_offset = offset;
    m_size = size;
}

void Transform::setScale(float v)
{
    mrCheckDirty(m_scale == v);
    m_scale = v;
}

void Transform::setGrayscale(bool v)
{
    mrCheckDirty(m_grayscale == v);
    m_grayscale = v;
}

ITexture2DPtr Transform::getDst()
{
    return m_dst;
}

void Transform::dispatch()
{
    if (!m_src)
        return;

    if (!m_dst) {
        auto size = m_src->getSize();
        if (m_scale != 1.0f)
            size = int2(float2(size) * m_scale);
        m_dst = Texture2D::create(size.x, size.y, m_grayscale ? TextureFormat::Ru8 : m_src->getFormat());
    }

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
        params.pixel_offset = params.pixel_size * m_offset;
        params.sample_step = (float2(m_size) / float2(src_size)) / float2(dst_size);
        params.flip_rb = 0;
        params.grayscale = m_grayscale ? 1 : 0;

        m_const = Buffer::createConstant(params);
        m_dirty = false;
    }
    m_cs->dispatch(*this);
}



ContourCS::ContourCS()
{
    m_cs.initialize(mrBytecode(g_hlsl_Contour));
}

void ContourCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<Contour&>(ctx_);

    m_cs.setSRV(ctx.m_src);
    m_cs.setUAV(ctx.m_dst);
    m_cs.setCBuffer(ctx.m_const);

    auto size = ctx.m_dst->getSize();
    m_cs.dispatch(
        ceildiv(size.x, 32),
        ceildiv(size.y, 32));
}

ContourPtr ContourCS::createContext()
{
    return make_ref<Contour>(this);

}

Contour::Contour(ContourCS* v)
    : m_cs(v)
{
}

void Contour::setSrc(ITexture2DPtr v)
{
    m_src = cast(v);
}

void Contour::setDst(ITexture2DPtr v)
{
    m_dst = cast(v);
}

void Contour::setBlockSize(int v)
{
    mrCheckDirty(v == m_block_size);
    m_block_size = v;
}

ITexture2DPtr Contour::getDst()
{
    return m_dst;
}

void Contour::dispatch()
{
    if (!m_src)
        return;

    if (!m_dst) {
        auto size = m_src->getSize();
        m_dst = Texture2D::create(size.x, size.y, TextureFormat::Ru8);
    }

    if (m_dirty) {
        struct
        {
            int range;
            int3 pad;
        } params{};
        params.range = (m_block_size - 1) / 2;

        m_const = Buffer::createConstant(params);
        m_dirty = false;
    }

    m_cs->dispatch(*this);
}



BinarizeCS::BinarizeCS()
{
    m_cs.initialize(mrBytecode(g_hlsl_Binarize));
}

void BinarizeCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<Binarize&>(ctx_);

    m_cs.setSRV(ctx.m_src);
    m_cs.setUAV(ctx.m_dst);
    m_cs.setCBuffer(ctx.m_const);

    auto size = ctx.m_dst->getSize();
    m_cs.dispatch(
        size.x,
        ceildiv(size.y, 32));
}

BinarizePtr BinarizeCS::createContext()
{
    return make_ref<Binarize>(this);
}

Binarize::Binarize(BinarizeCS* v)
    : m_cs(v)
{
}

void Binarize::setSrc(ITexture2DPtr v)
{
    m_src = cast(v);
}

void Binarize::setDst(ITexture2DPtr v)
{
    m_dst = cast(v);
}

void Binarize::setThreshold(float v)
{
    mrCheckDirty(v == m_threshold);
    m_threshold = v;
}

ITexture2DPtr Binarize::getDst()
{
    return m_dst;
}

void Binarize::dispatch()
{
    if (!m_src)
        return;

    if (!m_dst) {
        auto size = m_src->getSize();
        m_dst = Texture2D::create(ceildiv(size.x, 32), size.y, TextureFormat::Ri32);
    }

    if (m_dirty) {
        struct
        {
            float threshold;
            int3 pad;
        } params{};
        params.threshold = m_threshold;

        m_const = Buffer::createConstant(params);
        m_dirty = false;
    }

    m_cs->dispatch(*this);
}



TemplateMatchCS::TemplateMatchCS()
{
    m_cs_grayscale.initialize(mrBytecode(g_hlsl_TemplateMatch_Grayscale));
    m_cs_binary.initialize(mrBytecode(g_hlsl_TemplateMatch_Binary));
}

void TemplateMatchCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<TemplateMatch&>(ctx_);
    auto& src = ctx.m_src;
    auto& dst = ctx.m_dst;
    auto& tmp = ctx.m_template;

    auto size = dst->getSize();
    if (src->getFormat() == TextureFormat::Ru8) {
        m_cs_grayscale.setSRV(src, 0);
        m_cs_grayscale.setSRV(tmp, 1);
        m_cs_grayscale.setUAV(dst);

        m_cs_grayscale.dispatch(
            ceildiv(size.x, 32),
            ceildiv(size.y, 32));
    }
    else if (src->getFormat() == TextureFormat::Ri32) {
        m_cs_binary.setSRV(src, 0);
        m_cs_binary.setSRV(tmp, 1);
        m_cs_binary.setUAV(dst);

        m_cs_binary.dispatch(
            ceildiv(size.x, 32),
            ceildiv(size.y, 32));
    }
}

TemplateMatchPtr TemplateMatchCS::createContext()
{
    return make_ref<TemplateMatch>(this);
}

TemplateMatch::TemplateMatch(TemplateMatchCS* v)
    : m_cs(v)
{
}

void TemplateMatch::setSrc(ITexture2DPtr v)
{
    m_src = cast(v);
}

void TemplateMatch::setDst(ITexture2DPtr v)
{
    m_dst = cast(v);
}

void TemplateMatch::setTemplate(ITexture2DPtr v)
{
    m_template = cast(v);
}

ITexture2DPtr TemplateMatch::getDst()
{
    return m_dst;
}

void TemplateMatch::dispatch()
{
    if (!m_src || !m_template)
        return;
    if (m_src->getFormat() != m_template->getFormat()) {
        mrDbgPrint("*** GfxInterface::templateMatch(): format mismatch ***\n");
        return;
    }

    if (!m_dst) {
        if (m_src->getFormat() == TextureFormat::Ru8) {
            auto size = m_src->getSize() - m_template->getSize();
            m_dst = Texture2D::create(size.x, size.y, TextureFormat::Rf32);
        }
        else if (m_src->getFormat() == TextureFormat::Ri32) {
            auto size = m_src->getSize() - m_template->getSize();
            m_dst = Texture2D::create(size.x * 32, size.y, TextureFormat::Rf32);
        }
        if (!m_dst)
            return;
    }

    m_cs->dispatch(*this);
}



ReduceTotalCS::ReduceTotalCS()
{
    m_cs_fpass1.initialize(mrBytecode(g_hlsl_ReduceTotal_FPass1));
    m_cs_fpass2.initialize(mrBytecode(g_hlsl_ReduceTotal_FPass2));
    m_cs_ipass1.initialize(mrBytecode(g_hlsl_ReduceTotal_IPass1));
    m_cs_ipass2.initialize(mrBytecode(g_hlsl_ReduceTotal_IPass2));
}

void ReduceTotalCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<ReduceTotal&>(ctx_);

    auto do_dispatch = [this, &ctx](auto& pass1, auto& pass2) {
        auto size = ctx.m_src->getSize();
        pass1.setSRV(ctx.m_src);
        pass1.setUAV(ctx.m_dst);
        pass2.setSRV(ctx.m_src);
        pass2.setUAV(ctx.m_dst);
        pass1.dispatch(1, size.y);
        pass2.dispatch(1, 1);
    };

    if (IsIntFormat(ctx.m_src->getFormat()))
        do_dispatch(m_cs_ipass1, m_cs_ipass2);
    else
        do_dispatch(m_cs_fpass1, m_cs_fpass2);
}

ReduceTotalPtr ReduceTotalCS::createContext()
{
    return make_ref<ReduceTotal>(this);
}

ReduceTotal::ReduceTotal(ReduceTotalCS* v)
    : m_cs(v)
{
}

void ReduceTotal::setSrc(ITexture2DPtr v)
{
    m_src = cast(v);
}

ReduceTotal::Result ReduceTotal::getResult()
{
    Result ret{};
    if (!m_dst)
        return ret;

    m_dst->map([&ret](const void* v) {
        ret = *(Result*)v;
        });
    return ret;
}

void ReduceTotal::dispatch()
{
    if (!m_src)
        return;

    size_t rsize = m_src->getSize().y * sizeof(float);
    if (!m_dst || m_dst->getSize() != rsize) {
        m_dst = Buffer::createStructured(rsize, sizeof(float));
    }

    m_cs->dispatch(*this);
    m_dst->download(sizeof(float));
}



ReduceCountBitsCS::ReduceCountBitsCS()
{
    m_cs_pass1.initialize(mrBytecode(g_hlsl_ReduceCountBits_Pass1));
    m_cs_pass2.initialize(mrBytecode(g_hlsl_ReduceCountBits_Pass2));
}

void ReduceCountBitsCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<ReduceCountBits&>(ctx_);

    m_cs_pass1.setSRV(ctx.m_src);
    m_cs_pass1.setUAV(ctx.m_dst);
    m_cs_pass2.setSRV(ctx.m_src);
    m_cs_pass2.setUAV(ctx.m_dst);

    auto image_size = ctx.m_src->getSize();
    m_cs_pass1.dispatch(1, image_size.y);
    m_cs_pass2.dispatch(1, 1);
}

ReduceCountBitsPtr ReduceCountBitsCS::createContext()
{
    return make_ref<ReduceCountBits>(this);
}

ReduceCountBits::ReduceCountBits(ReduceCountBitsCS* v)
    : m_cs(v)
{
}

void ReduceCountBits::setSrc(ITexture2DPtr v)
{
    m_src = cast(v);
}

uint32_t ReduceCountBits::getResult()
{
    uint32_t ret{};
    if (!m_dst)
        return ret;

    m_dst->map([&ret](const void* v) {
        ret = *(uint32_t*)v;
        });
    return ret;
}

void ReduceCountBits::dispatch()
{
    if (!m_src)
        return;

    size_t rsize = m_src->getSize().y * sizeof(uint32_t);
    if (!m_dst || m_dst->getSize() != rsize) {
        m_dst = Buffer::createStructured(rsize, sizeof(uint32_t));
    }

    m_cs->dispatch(*this);
    m_dst->download(sizeof(uint32_t));
}



ReduceMinMaxCS::ReduceMinMaxCS()
{
    m_cs_fpass1.initialize(mrBytecode(g_hlsl_ReduceMinMax_FPass1));
    m_cs_fpass2.initialize(mrBytecode(g_hlsl_ReduceMinMax_FPass2));
    m_cs_ipass1.initialize(mrBytecode(g_hlsl_ReduceMinMax_IPass1));
    m_cs_ipass2.initialize(mrBytecode(g_hlsl_ReduceMinMax_IPass2));
}

void ReduceMinMaxCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<ReduceMinMax&>(ctx_);

    auto do_dispatch = [this, &ctx](auto& pass1, auto& pass2) {
        auto size = ctx.m_src->getSize();
        pass1.setSRV(ctx.m_src);
        pass1.setUAV(ctx.m_dst);
        pass2.setSRV(ctx.m_src);
        pass2.setUAV(ctx.m_dst);
        pass1.dispatch(1, size.y);
        pass2.dispatch(1, 1);
    };

    if (IsIntFormat(ctx.m_src->getFormat()))
        do_dispatch(m_cs_ipass1, m_cs_ipass2);
    else
        do_dispatch(m_cs_fpass1, m_cs_fpass2);
}

ReduceMinMaxPtr ReduceMinMaxCS::createContext()
{
    return make_ref<ReduceMinMax>(this);
}

ReduceMinMax::ReduceMinMax(ReduceMinMaxCS* v)
    : m_cs(v)
{
}

void ReduceMinMax::setSrc(ITexture2DPtr v)
{
    m_src = cast(v);
}

ReduceMinMax::Result ReduceMinMax::getResult()
{
    Result ret{};
    if (!m_dst)
        return ret;

    m_dst->map([&ret](const void* v) {
        ret = *(Result*)v;
        });
    return ret;
}

void ReduceMinMax::dispatch()
{
    if (!m_src)
        return;

    size_t rsize = m_src->getSize().y * sizeof(Result);
    if (!m_dst || m_dst->getSize() != rsize) {
        m_dst = Buffer::createStructured(rsize, sizeof(Result));
    }

    m_cs->dispatch(*this);
    m_dst->download(sizeof(Result));
}


} // namespace mr
