#include "pch.h"
#include "Graphics/Shader.h"

// shader binaries
#include "Transform.hlsl.h"
#include "Normalize_F.hlsl.h"
#include "Normalize_I.hlsl.h"
#include "Binarize.hlsl.h"
#include "Contour.hlsl.h"
#include "TemplateMatch_Grayscale.hlsl.h"
#include "TemplateMatch_Binary.hlsl.h"

#define mrBytecode(A) A, std::size(A)

#define mrCheckDirty(...)\
    if (__VA_ARGS__) { return; }\
    m_dirty = true;


namespace mr {

class Transform : public RefCount<ITransform>
{
public:
    Transform(TransformCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setRect(int2 o, int2 s) override;
    void setScale(float v) override;
    void setGrayscale(bool v) override;
    void setFiltering(bool v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    TransformCS* m_cs{};
    Texture2DPtr m_src;
    Texture2DPtr m_dst;
    BufferPtr m_const;

    int2 m_offset = int2::zero();
    int2 m_size = int2::zero();
    float m_scale = 1.0f;
    bool m_grayscale = false;
    bool m_filtering = false;
    bool m_dirty = true;
};

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

void Transform::setFiltering(bool v)
{
    mrCheckDirty(m_filtering == v);
    m_filtering = v;
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
            int grayscale;
            int filtering;
        } params;
        params.pixel_size = 1.0f / float2(src_size);
        params.pixel_offset = params.pixel_size * m_offset;
        params.sample_step = (float2(m_size) / float2(src_size)) / float2(dst_size);
        params.grayscale = m_grayscale ? 1 : 0;
        params.filtering = m_filtering ? 1 : 0;

        m_const = Buffer::createConstant(params);
        m_dirty = false;
    }
    m_cs->dispatch(*this);
}

TransformCS::TransformCS()
{
    m_cs.initialize(mrBytecode(g_hlsl_Transform));
    m_cs.setSampler(mrGfxDefaultSampler());
}

void TransformCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<Transform&>(ctx_);

    m_cs.setCBuffer(ctx.m_const);
    m_cs.setSRV(ctx.m_src);
    m_cs.setUAV(ctx.m_dst);

    int2 dst_size = ctx.m_dst->getSize();
    m_cs.dispatch(
        ceildiv(dst_size.x, 32),
        ceildiv(dst_size.y, 32));
}

ITransformPtr TransformCS::createContext()
{
    return make_ref<Transform>(this);
}



class Normalize : public RefCount<INormalize>
{
public:
    Normalize(NormalizeCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setMax(float v) override;
    void setMax(uint32_t v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    NormalizeCS* m_cs{};
    Texture2DPtr m_dst;
    Texture2DPtr m_src;
    BufferPtr m_const;

    float m_rmax= 1.0f;
    bool m_dirty = true;
};

Normalize::Normalize(NormalizeCS* v)
    : m_cs(v)
{
}

void Normalize::setSrc(ITexture2DPtr v)
{
    m_src = cast(v);
}

void Normalize::setDst(ITexture2DPtr v)
{
    m_dst = cast(v);
}

void Normalize::setMax(float v_)
{
    float v = 1.0f / v_;
    mrCheckDirty(m_rmax == v);
    m_rmax = v;
}

void Normalize::setMax(uint32_t v_)
{
    setMax(float(v_));
}

ITexture2DPtr Normalize::getDst()
{
    return m_dst;
}

void Normalize::dispatch()
{
    if (!m_src)
        return;

    if (!m_dst) {
        auto size = m_src->getSize();
        m_dst = Texture2D::create(size.x, size.y, TextureFormat::Rf32);
    }
    if (m_dirty) {
        struct
        {
            float rmax;
            int3 pad;
        } params{};
        params.rmax = m_rmax;

        m_const = Buffer::createConstant(params);
        m_dirty = false;
    }

    m_cs->dispatch(*this);
}

NormalizeCS::NormalizeCS()
{
    m_cs_f.initialize(mrBytecode(g_hlsl_Normalize_F));
    m_cs_i.initialize(mrBytecode(g_hlsl_Normalize_I));
}

void NormalizeCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<Normalize&>(ctx_);

    auto size = ctx.m_dst->getSize();
    if (IsIntFormat(ctx.m_src->getFormat())) {
        m_cs_i.setCBuffer(ctx.m_const);
        m_cs_i.setSRV(ctx.m_src);
        m_cs_i.setUAV(ctx.m_dst);
        m_cs_i.dispatch(
            ceildiv(size.x, 32),
            ceildiv(size.y, 32));
    }
    else {
        m_cs_f.setCBuffer(ctx.m_const);
        m_cs_f.setSRV(ctx.m_src);
        m_cs_f.setUAV(ctx.m_dst);
        m_cs_f.dispatch(
            ceildiv(size.x, 32),
            ceildiv(size.y, 32));
    }
}

INormalizePtr NormalizeCS::createContext()
{
    return make_ref<Normalize>(this);
}


class Binarize : public RefCount<IBinarize>
{
public:
    Binarize(BinarizeCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setThreshold(float v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    BinarizeCS* m_cs{};
    Texture2DPtr m_dst;
    Texture2DPtr m_src;
    BufferPtr m_const;

    float m_threshold = 0.5f;
    bool m_dirty = true;
};

IBinarizePtr BinarizeCS::createContext()
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


class Contour : public RefCount<IContour>
{
public:
    Contour(ContourCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setBlockSize(int v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    ContourCS* m_cs{};
    Texture2DPtr m_dst;
    Texture2DPtr m_src;
    BufferPtr m_const;

    int m_block_size = 5;
    bool m_dirty = true;
};

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

IContourPtr ContourCS::createContext()
{
    return make_ref<Contour>(this);
}


class TemplateMatch : public RefCount<ITemplateMatch>
{
public:
    TemplateMatch(TemplateMatchCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setTemplate(ITexture2DPtr v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    TemplateMatchCS* m_cs{};
    Texture2DPtr m_dst;
    Texture2DPtr m_src;
    Texture2DPtr m_template;
};

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
            m_dst = Texture2D::create(size.x * 32, size.y, TextureFormat::Ri32);
        }
        if (!m_dst)
            return;
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

ITemplateMatchPtr TemplateMatchCS::createContext()
{
    return make_ref<TemplateMatch>(this);
}

} // namespace mr
