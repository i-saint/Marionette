#include "pch.h"
#include "Graphics/mrShader.h"

// shader binaries
#include "Transform.hlsl.h"
#include "Bias.hlsl.h"
#include "Normalize_F.hlsl.h"
#include "Normalize_I.hlsl.h"
#include "Binarize.hlsl.h"
#include "Contour.hlsl.h"
#include "Expand_Grayscale.hlsl.h"
#include "Expand_Binary.hlsl.h"
#include "TemplateMatch_Grayscale.hlsl.h"
#include "TemplateMatch_Binary.hlsl.h"
#include "Shape.hlsl.h"

#define mrBytecode(A) A, std::size(A)

#define mrCheckDirty(...)\
    if (__VA_ARGS__) { return; }\
    m_dirty = true;


namespace mr {

template<class T>
class FilterCommon : public RefCount<T>
{
public:
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    ITexture2DPtr getDst() const override;

public:
    Texture2DPtr m_src;
    Texture2DPtr m_dst;
};

template<class T> void FilterCommon<T>::setSrc(ITexture2DPtr v) { m_src = cast(v); }
template<class T> void FilterCommon<T>::setDst(ITexture2DPtr v) { m_dst = cast(v); }
template<class T> ITexture2DPtr FilterCommon<T>::getDst() const { return m_dst; }


class Transform : public FilterCommon<ITransform>
{
public:
    enum class Flag
    {
        Grayscale,
        FillAlpha,
    };

    Transform(TransformCS* v);
    void setDstFormat(TextureFormat v) override;
    void setSrcRegion(Rect v) override;
    void setScale(float v) override;
    void setGrayscale(bool v) override;
    void setFillAlpha(bool v) override;
    void setFiltering(bool v) override;
    void dispatch() override;

public:
    TransformCS* m_cs{};
    BufferPtr m_const;

    TextureFormat m_dst_format = TextureFormat::Unknown;
    Rect m_region{};
    float m_scale = 1.0f;
    bool m_grayscale = false;
    bool m_fill_alpha = false;
    bool m_filtering = false;
    bool m_dirty = true;
};

Transform::Transform(TransformCS* v) : m_cs(v) {}
void Transform::setDstFormat(TextureFormat v) { m_dst_format = v; }
void Transform::setSrcRegion(Rect v) { mrCheckDirty(m_region == v); m_region = v; }
void Transform::setScale(float v) { mrCheckDirty(m_scale == v); m_scale = v; }
void Transform::setGrayscale(bool v) { mrCheckDirty(m_grayscale == v); m_grayscale = v; }
void Transform::setFillAlpha(bool v) { mrCheckDirty(m_fill_alpha == v); m_fill_alpha = v; }
void Transform::setFiltering(bool v) { mrCheckDirty(m_filtering == v); m_filtering = v; }

void Transform::dispatch()
{
    if (!m_src)
        return;

    if (!m_dst) {
        auto size = m_src->getSize();
        if (m_scale != 1.0f)
            size = int2(float2(size) * m_scale);

        TextureFormat format = m_dst_format;
        if (format == TextureFormat::Unknown)
            format = m_grayscale ? TextureFormat::Ru8 : m_src->getFormat();
        m_dst = Texture2D::create(size.x, size.y, format);
    }

    if (m_dirty) {
        int2 src_size = m_src->getSize();
        int2 dst_size = m_dst->getSize();
        int2 size = m_region.size == int2::zero() ? src_size : m_region.size;

        struct
        {
            float2 pixel_size;
            float2 pixel_offset;
            float2 sample_step;
            uint32_t flags;
            int filter;
        } params{};
        params.pixel_size = 1.0f / float2(src_size);
        params.pixel_offset = params.pixel_size * m_region.pos;
        params.sample_step = (float2(size) / float2(src_size)) / float2(dst_size);
        if (m_grayscale)
            set_flag(params.flags, Flag::Grayscale, true);
        if (m_fill_alpha)
            set_flag(params.flags, Flag::FillAlpha, true);
        params.filter = 0;
        if (m_filtering) {
            if (dst_size.x < src_size.x / 3) params.filter = 4;
            else if (dst_size.x < src_size.x / 2) params.filter = 3;
            else if (dst_size.x < src_size.x / 1) params.filter = 2;
        }

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

void TransformCS::dispatch(ICSContext& ctx)
{
    auto& c = static_cast<Transform&>(ctx);

    m_cs.setCBuffer(c.m_const);
    m_cs.setSRV(c.m_src);
    m_cs.setUAV(c.m_dst);

    auto dst_size = c.m_dst->getInternalSize();
    m_cs.dispatch(
        ceildiv(dst_size.x, 32),
        ceildiv(dst_size.y, 32));
}

ITransformPtr TransformCS::createContext()
{
    return make_ref<Transform>(this);
}


class Bias : public FilterCommon<IBias>
{
public:
    Bias(BiasCS* v);
    void setBias(float v) override;
    void dispatch() override;

public:
    BiasCS* m_cs{};
    BufferPtr m_const;

    float m_bias = 0.0f;
    bool m_dirty = true;
};

Bias::Bias(BiasCS* v) : m_cs(v) {}

void Bias::setBias(float v)
{
    mrCheckDirty(m_bias == v);
    m_bias = v;
}

void Bias::dispatch()
{
    if (!m_src)
        return;

    if (!m_dst) {
        auto size = m_src->getSize();
        m_dst = Texture2D::create(size.x, size.y, m_src->getFormat());
    }
    if (m_dirty) {
        struct
        {
            float bias;
            float mul;
            int2 pad;
        } params{};
        params.bias = m_bias;
        params.mul = 1.0f / (1.0f + m_bias);

        m_const = Buffer::createConstant(params);
        m_dirty = false;
    }

    m_cs->dispatch(*this);
}

BiasCS::BiasCS()
{
    m_cs.initialize(mrBytecode(g_hlsl_Bias));
}

void BiasCS::dispatch(ICSContext& ctx)
{
    auto& c = static_cast<Bias&>(ctx);

    auto size = c.m_dst->getInternalSize();
    m_cs.setCBuffer(c.m_const);
    m_cs.setSRV(c.m_src);
    m_cs.setUAV(c.m_dst);
    m_cs.dispatch(
        ceildiv(size.x, 32),
        ceildiv(size.y, 32));
}

IBiasPtr BiasCS::createContext()
{
    return make_ref<Bias>(this);
}


class Normalize : public FilterCommon<INormalize>
{
public:
    Normalize(NormalizeCS* v);
    void setMax(float v) override;
    void setMax(uint32_t v) override;
    void dispatch() override;

public:
    NormalizeCS* m_cs{};
    BufferPtr m_const;

    float m_rmax = 1.0f;
    bool m_dirty = true;
};

Normalize::Normalize(NormalizeCS* v) : m_cs(v) {}
void Normalize::setMax(float v_) { float v = 1.0f / v_; mrCheckDirty(m_rmax == v); m_rmax = v; }
void Normalize::setMax(uint32_t v_) { setMax(float(v_)); }

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

void NormalizeCS::dispatch(ICSContext& ctx)
{
    auto& c = static_cast<Normalize&>(ctx);

    auto size = c.m_dst->getInternalSize();
    auto& cs = IsIntFormat(c.m_src->getFormat()) ? m_cs_i : m_cs_f;
    cs.setCBuffer(c.m_const);
    cs.setSRV(c.m_src);
    cs.setUAV(c.m_dst);
    cs.dispatch(
        ceildiv(size.x, 32),
        ceildiv(size.y, 32));
}

INormalizePtr NormalizeCS::createContext()
{
    return make_ref<Normalize>(this);
}


class Binarize : public FilterCommon<IBinarize>
{
public:
    Binarize(BinarizeCS* v);
    void setThreshold(float v) override;
    void dispatch() override;

public:
    BinarizeCS* m_cs{};
    BufferPtr m_const;

    float m_threshold = 0.5f;
    bool m_dirty = true;
};

IBinarizePtr BinarizeCS::createContext()
{
    return make_ref<Binarize>(this);
}

Binarize::Binarize(BinarizeCS* v) : m_cs(v) {}
void Binarize::setThreshold(float v) { mrCheckDirty(v == m_threshold); m_threshold = v; }

void Binarize::dispatch()
{
    if (!m_src)
        return;

    if (!m_dst) {
        auto size = m_src->getSize();
        m_dst = Texture2D::create(size.x, size.y, TextureFormat::Binary);
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

void BinarizeCS::dispatch(ICSContext& ctx)
{
    auto& c = static_cast<Binarize&>(ctx);

    m_cs.setSRV(c.m_src);
    m_cs.setUAV(c.m_dst);
    m_cs.setCBuffer(c.m_const);

    auto size = c.m_dst->getInternalSize();
    m_cs.dispatch(
        size.x,
        ceildiv(size.y, 32));
}


class Contour : public FilterCommon<IContour>
{
public:
    Contour(ContourCS* v);
    void setRadius(float v) override;
    void dispatch() override;

public:
    ContourCS* m_cs{};
    BufferPtr m_const;

    float m_radius = 1.0f;
    float m_strength = 1.0f;
    bool m_dirty = true;
};

Contour::Contour(ContourCS* v) : m_cs(v) {}
void Contour::setRadius(float v) { mrCheckDirty(v == m_radius); m_radius = v; }

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
            float radius;
            float strength;
            int2 pad;
        } params{};
        params.radius = m_radius;
        params.strength = m_strength;

        m_const = Buffer::createConstant(params);
        m_dirty = false;
    }

    m_cs->dispatch(*this);
}

ContourCS::ContourCS()
{
    m_cs.initialize(mrBytecode(g_hlsl_Contour));
}

void ContourCS::dispatch(ICSContext& ctx)
{
    auto& c = static_cast<Contour&>(ctx);

    m_cs.setSRV(c.m_src);
    m_cs.setUAV(c.m_dst);
    m_cs.setCBuffer(c.m_const);

    auto size = c.m_dst->getInternalSize();
    m_cs.dispatch(
        ceildiv(size.x, 32),
        ceildiv(size.y, 32));
}

IContourPtr ContourCS::createContext()
{
    return make_ref<Contour>(this);
}


class Expand : public FilterCommon<IExpand>
{
public:
    Expand(ExpandCS* v);
    void setRadius(float v) override;
    void dispatch() override;

public:
    ExpandCS* m_cs{};
    BufferPtr m_const;

    float m_radius = 1.0f;
    bool m_dirty = true;
};

Expand::Expand(ExpandCS* v) : m_cs(v) {}
void Expand::setRadius(float v) { mrCheckDirty(v == m_radius); m_radius = v; }

void Expand::dispatch()
{
    if (!m_src)
        return;

    if (m_dst && m_dst->getFormat() != m_src->getFormat()) {
        m_dst = nullptr;
    }
    if (!m_dst) {
        auto size = m_src->getSize();
        m_dst = Texture2D::create(size.x, size.y, m_src->getFormat());
    }

    if (m_dirty) {
        struct
        {
            float radius;
            int3 pad;
        } params{};
        params.radius = m_radius;

        m_const = Buffer::createConstant(params);
        m_dirty = false;
    }

    m_cs->dispatch(*this);
}

ExpandCS::ExpandCS()
{
    m_cs_grayscale.initialize(mrBytecode(g_hlsl_Expand_Grayscale));
    m_cs_binary.initialize(mrBytecode(g_hlsl_Expand_Binary));
}

void ExpandCS::dispatch(ICSContext& ctx)
{
    auto& c = static_cast<Expand&>(ctx);

    auto size = c.m_dst->getInternalSize();
    if (c.m_src->getFormat() == TextureFormat::Binary) {
        m_cs_binary.setSRV(c.m_src);
        m_cs_binary.setUAV(c.m_dst);
        m_cs_binary.setCBuffer(c.m_const);
        m_cs_binary.dispatch(
            ceildiv(size.x, 32),
            ceildiv(size.y, 32));
    }
    else {
        m_cs_grayscale.setSRV(c.m_src);
        m_cs_grayscale.setUAV(c.m_dst);
        m_cs_grayscale.setCBuffer(c.m_const);
        m_cs_grayscale.dispatch(
            ceildiv(size.x, 32),
            ceildiv(size.y, 32));
    }
}

IExpandPtr ExpandCS::createContext()
{
    return make_ref<Expand>(this);
}


class TemplateMatch : public FilterCommon<ITemplateMatch>
{
public:
    TemplateMatch(TemplateMatchCS* v);
    void setTemplate(ITexture2DPtr v) override;
    void setMask(ITexture2DPtr v) override;
    void setRegion(Rect v) override;
    void setFitDstSize(bool v) override;
    void dispatch() override;

    int2 getSize() const;

public:
    TemplateMatchCS* m_cs{};
    Texture2DPtr m_template;
    Texture2DPtr m_mask;
    BufferPtr m_const;

    int2 m_template_size{};
    Rect m_region{};
    bool m_fit_dst_size = true;
    bool m_dirty = true;
};

TemplateMatch::TemplateMatch(TemplateMatchCS* v) : m_cs(v) {}

void TemplateMatch::setTemplate(ITexture2DPtr v) {
    m_template = cast(v);
    mrCheckDirty(m_template_size == m_template->getSize());
    m_template_size = m_template->getSize();
}

void TemplateMatch::setMask(ITexture2DPtr v)
{
    m_mask = cast(v);
}

void TemplateMatch::setRegion(Rect v)
{
    mrCheckDirty(m_region == v);
    m_region = v;
}

void TemplateMatch::setFitDstSize(bool v)
{
    m_fit_dst_size = v;
}

int2 TemplateMatch::getSize() const
{
    return m_region.size.x == 0 ? (m_src ? m_src->getSize() : int2::zero()) : m_region.size;
}

void TemplateMatch::dispatch()
{
    if (!m_src || !m_template)
        return;
    if (m_src->getFormat() != m_template->getFormat()) {
        mrDbgPrint("*** TemplateMatch::dispatch(): format mismatch ***\n");
        return;
    }

    if (!m_dst) {
        int2 size;
        if (m_fit_dst_size)
            size = getSize() - m_template->getSize();
        else
            size = m_src->getSize();
        auto format = m_src->getFormat() == TextureFormat::Binary ? TextureFormat::Ri32 : TextureFormat::Rf32;
        m_dst = Texture2D::create(size.x, size.y, format);
    }

    if (m_dirty) {
        struct
        {
            int2 range;
            int2 tl;
            int2 br;
            int2 template_size;
        } params{};

        params.range = getSize();
        params.tl = m_region.pos;
        params.br = params.tl + params.range;
        params.template_size = m_template_size;

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

void TemplateMatchCS::dispatch(ICSContext& ctx)
{
    auto& c = static_cast<TemplateMatch&>(ctx);

    auto size = c.getSize();
    if (size.x < 0 || size.y < 0) {
        mrDbgPrint("*** TemplateMatchCS::dispatch(): size < 0 ***\n");
        return;
    }

    auto& cs = c.m_src->getFormat() == TextureFormat::Binary ? m_cs_binary : m_cs_grayscale;
    cs.setCBuffer(c.m_const, 0);
    cs.setSRV(c.m_src, 0);
    cs.setSRV(c.m_template, 1);
    cs.setSRV(c.m_mask, 2);
    cs.setUAV(c.m_dst);
    cs.dispatch(
        ceildiv(size.x, 32),
        ceildiv(size.y, 32));
}

ITemplateMatchPtr TemplateMatchCS::createContext()
{
    return make_ref<TemplateMatch>(this);
}



class Shape : public RefCount<IShape>
{
public:
    enum class ShapeType : int
    {
        Unknown,
        Circle,
        Rect,
    };
    struct ShapeData
    {
        ShapeType type;
        float border;
        int2 pos;
        float4 color;
        float radius;
        int2 rect_size;
        int pad;
    };


    Shape(ShapeCS* v);
    void setDst(ITexture2DPtr v) override;
    void addCircle(int2 pos, float radius, float border, float4 color) override;
    void addRect(Rect rect, float border, float4 color) override;
    void clearShapes() override;
    void dispatch() override;

public:
    ShapeCS* m_cs{};
    Texture2DPtr m_dst;
    BufferPtr m_buffer;

    std::vector<ShapeData> m_shapes;
    bool m_dirty = true;
};

Shape::Shape(ShapeCS* v) : m_cs(v) {}
void Shape::setDst(ITexture2DPtr v){ m_dst = cast(v); }

void Shape::addCircle(int2 pos, float radius, float border, float4 color)
{
    ShapeData tmp;
    tmp.type = ShapeType::Circle;
    tmp.pos = pos;
    tmp.radius = radius;
    tmp.border = border;
    tmp.color = color;
    m_shapes.push_back(tmp);
    m_dirty = true;
}

void Shape::addRect(Rect rect, float border, float4 color)
{
    ShapeData tmp;
    tmp.type = ShapeType::Rect;
    tmp.pos = rect.pos;
    tmp.rect_size = rect.size;
    tmp.border = border;
    tmp.color = color;
    m_shapes.push_back(tmp);
    m_dirty = true;
}

void Shape::clearShapes()
{
    m_shapes.clear();
    m_dirty = true;
}

void Shape::dispatch()
{
    if (!m_dst || m_shapes.empty())
        return;

    if (m_dirty) {
        m_buffer = Buffer::createStructured(m_shapes.size() * sizeof(ShapeData), sizeof(ShapeData), m_shapes.data());
        m_dirty = false;
    }

    m_cs->dispatch(*this);
}

mr::ShapeCS::ShapeCS()
{
    m_cs.initialize(mrBytecode(g_hlsl_Shape));
}

void mr::ShapeCS::dispatch(ICSContext& ctx)
{
    auto& c = static_cast<Shape&>(ctx);

    auto size = c.m_dst->getInternalSize();
    m_cs.setSRV(c.m_buffer);
    m_cs.setUAV(c.m_dst);
    m_cs.dispatch(
        ceildiv(size.x, 32),
        ceildiv(size.y, 32));
}

IShapePtr mr::ShapeCS::createContext()
{
    return make_ref<Shape>(this);
}

} // namespace mr
