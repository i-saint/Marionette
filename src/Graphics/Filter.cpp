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
    auto& ctx = static_cast<TransformCtx&>(ctx_);
    m_cs.setSRV(ctx.m_src);
    m_cs.setUAV(ctx.m_dst);
    m_cs.setCBuffer(ctx.m_const);
    m_cs.dispatch(
        ceildiv(ctx.m_size.x, 32),
        ceildiv(ctx.m_size.y, 32));
}

TransformCtx* TransformCS::createContext_()
{
    return new TransformCtx(this);
}

TransformCtx::TransformCtx(TransformCS* v)
    : m_cs(v)
{
}

void TransformCtx::setSrc(ITexture2DPtr v)
{
    m_src = i2c(v);
}

void TransformCtx::setDst(ITexture2DPtr v)
{
    m_dst = i2c(v);
}

void TransformCtx::setRect(int2 offset, int2 size)
{
    mrCheckDirty(offset == m_offset && size == m_size);
    m_offset = offset;
    m_size = size;
}

void TransformCtx::setScale(float v)
{
    mrCheckDirty(m_scale == v);
    m_scale = v;
}

void TransformCtx::setGrayscale(bool v)
{
    mrCheckDirty(m_grayscale == v);
    m_grayscale = v;
}

ITexture2DPtr TransformCtx::getDst()
{
    return m_dst;
}

void TransformCtx::dispatch()
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
    auto& ctx = static_cast<ContourCtx&>(ctx_);

    m_cs.setSRV(ctx.m_src);
    m_cs.setUAV(ctx.m_dst);
    m_cs.setCBuffer(ctx.m_const);

    auto size = ctx.m_dst->getSize();
    m_cs.dispatch(
        ceildiv(size.x, 32),
        ceildiv(size.y, 32));
}

ContourCtx* ContourCS::createContext_()
{
    return new ContourCtx(this);

}

ContourCtx::ContourCtx(ContourCS* v)
    : m_cs(v)
{
}

void ContourCtx::setSrc(ITexture2DPtr v)
{
    m_src = i2c(v);
}

void ContourCtx::setDst(ITexture2DPtr v)
{
    m_dst = i2c(v);
}

void ContourCtx::setBlockSize(int v)
{
    mrCheckDirty(v == m_block_size);
    m_block_size = v;
}

ITexture2DPtr ContourCtx::getDst()
{
    return m_dst;
}

void ContourCtx::dispatch()
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
    auto& ctx = static_cast<BinarizeCtx&>(ctx_);

    m_cs.setSRV(ctx.m_src);
    m_cs.setUAV(ctx.m_dst);
    m_cs.setCBuffer(ctx.m_const);

    auto size = ctx.m_dst->getSize();
    m_cs.dispatch(
        size.x,
        ceildiv(size.y, 32));
}

BinarizeCtx* BinarizeCS::createContext_()
{
    return new BinarizeCtx(this);
}

BinarizeCtx::BinarizeCtx(BinarizeCS* v)
    : m_cs(v)
{
}

void BinarizeCtx::setSrc(ITexture2DPtr v)
{
    m_src = i2c(v);
}

void BinarizeCtx::setDst(ITexture2DPtr v)
{
    m_dst = i2c(v);
}

void BinarizeCtx::setThreshold(float v)
{
    mrCheckDirty(v == m_threshold);
    m_threshold = v;
}

ITexture2DPtr BinarizeCtx::getDst()
{
    return m_dst;
}

void BinarizeCtx::dispatch()
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
    auto& ctx = static_cast<TemplateMatchCtx&>(ctx_);
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

TemplateMatchCtx* TemplateMatchCS::createContext_()
{
    return new TemplateMatchCtx(this);
}

TemplateMatchCtx::TemplateMatchCtx(TemplateMatchCS* v)
    : m_cs(v)
{
}

void TemplateMatchCtx::setSrc(ITexture2DPtr v)
{
    m_src = i2c(v);
}

void TemplateMatchCtx::setDst(ITexture2DPtr v)
{
    m_dst = i2c(v);
}

void TemplateMatchCtx::setTemplate(ITexture2DPtr v)
{
    m_template = i2c(v);
}

ITexture2DPtr TemplateMatchCtx::getDst()
{
    return m_dst;
}

void TemplateMatchCtx::dispatch()
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



ReduceMinMaxCS::ReduceMinMaxCS()
{
    m_cs_pass1.initialize(mrBytecode(g_hlsl_ReduceMinMax_Pass1));
    m_cs_pass2.initialize(mrBytecode(g_hlsl_ReduceMinMax_Pass2));
}

void ReduceMinMaxCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<ReduceMinMaxCtx&>(ctx_);
    using result_t = IReduceMinMax::Result;

    m_cs_pass1.setSRV(ctx.m_src);
    m_cs_pass1.setUAV(ctx.m_dst);
    m_cs_pass2.setSRV(ctx.m_src);
    m_cs_pass2.setUAV(ctx.m_dst);

    auto image_size = ctx.m_src->getSize();
    m_cs_pass1.dispatch(1, image_size.y);
    m_cs_pass2.dispatch(1, 1);
    DispatchCopy(ctx.m_staging, ctx.m_dst, sizeof(result_t));

    // deferred getter
    ctx.m_result = std::async(std::launch::deferred, [&ctx]() {
        result_t ret{};
        MapRead(ctx.m_staging->ptr(), [&ret](const void* v) {
            ret = *(result_t*)v;
            });
        return ret;
        });
}

ReduceMinMaxCtx* ReduceMinMaxCS::createContext_()
{
    return new ReduceMinMaxCtx(this);
}

ReduceMinMaxCtx::ReduceMinMaxCtx(ReduceMinMaxCS* v)
    : m_cs(v)
{
}

void ReduceMinMaxCtx::setSrc(ITexture2DPtr v)
{
    m_src = i2c(v);
}

std::future<IReduceMinMax::Result>& ReduceMinMaxCtx::getResult()
{
    return m_result;
}

void ReduceMinMaxCtx::dispatch()
{
    if (!m_src)
        return;

    using result_t = IReduceMinMax::Result;
    size_t rsize = m_src->getSize().y * sizeof(result_t);
    if (!m_dst || m_dst->getSize() != rsize) {
        m_dst = Buffer::createStructured(rsize, sizeof(result_t));
    }
    if (!m_staging) {
        m_staging = Buffer::createStaging(sizeof(result_t));
    }

    m_cs->dispatch(*this);
}

} // namespace mr
