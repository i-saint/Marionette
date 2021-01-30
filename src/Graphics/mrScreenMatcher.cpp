#include "pch.h"
#include "mrInternal.h"
#include "mrGfxFoundation.h"

namespace mr {

struct Filter : public RefCount<IObject>
{
public:
    Filter(IGfxInterfacePtr gfx);

    ITexture2DPtr copy(ITexture2DPtr src);
    ITexture2DPtr transform(ITexture2DPtr src, float scale, bool grayscale, bool filtering);
    ITexture2DPtr transform(ITexture2DPtr src, float scale, bool grayscale);
    ITexture2DPtr normalize(ITexture2DPtr src, float denom);
    ITexture2DPtr binarize(ITexture2DPtr src, float threshold);
    ITexture2DPtr contour(ITexture2DPtr src, int block_size);
    ITexture2DPtr expand(ITexture2DPtr src, int block_size);
    ITexture2DPtr match(ITexture2DPtr src, ITexture2DPtr tmp, ITexture2DPtr mask = nullptr);

    std::future<IReduceTotal::Result> total(ITexture2DPtr src);
    std::future<IReduceCountBits::Result> countBits(ITexture2DPtr src);
    std::future<IReduceMinMax::Result> minmax(ITexture2DPtr src);

public:
    IGfxInterfacePtr m_gfx;

    ITransformPtr m_transform;
    INormalizePtr m_normalize;
    IBinarizePtr m_binarize;
    IContourPtr m_contour;
    IExpandPtr m_expand;
    ITemplateMatchPtr m_match;

    IReduceTotalPtr m_total;
    IReduceCountBitsPtr m_count_bits;
    IReduceMinMaxPtr m_minmax;
};
mrDeclPtr(Filter);

Filter::Filter(IGfxInterfacePtr gfx)
    : m_gfx(gfx)
{
}
#define mrMakeFilter(N, T)\
    if (!N)\
        N = m_gfx->create##T();\
    auto& filter = N;


ITexture2DPtr Filter::copy(ITexture2DPtr src)
{
    mrMakeFilter(m_transform, Transform);
    filter->setSrc(src);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr Filter::transform(ITexture2DPtr src, float scale, bool grayscale, bool filtering)
{
    mrMakeFilter(m_transform, Transform);
    filter->setSrc(src);
    filter->setScale(scale);
    filter->setGrayscale(grayscale);
    filter->setFiltering(filtering);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr Filter::transform(ITexture2DPtr src, float scale, bool grayscale)
{
    return transform(src, scale, grayscale, scale < 1.0f);
}

ITexture2DPtr Filter::normalize(ITexture2DPtr src, float denom)
{
    mrMakeFilter(m_normalize, Normalize);
    filter->setSrc(src);
    filter->setMax(denom);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr Filter::binarize(ITexture2DPtr src, float threshold)
{
    mrMakeFilter(m_binarize, Binarize);
    filter->setSrc(src);
    filter->setThreshold(threshold);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr Filter::contour(ITexture2DPtr src, int block_size)
{
    mrMakeFilter(m_contour, Contour);
    filter->setSrc(src);
    filter->setBlockSize(block_size);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr Filter::expand(ITexture2DPtr src, int block_size)
{
    mrMakeFilter(m_expand, Expand);
    filter->setSrc(src);
    filter->setBlockSize(block_size);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr Filter::match(ITexture2DPtr src, ITexture2DPtr tmp, ITexture2DPtr mask)
{
    mrMakeFilter(m_match, TemplateMatch);
    filter->setSrc(src);
    filter->setTemplate(tmp);
    filter->setMask(mask);
    filter->dispatch();
    return filter->getDst();
}


std::future<IReduceTotal::Result> Filter::total(ITexture2DPtr src)
{
    mrMakeFilter(m_total, ReduceTotal);
    filter->setSrc(src);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}

std::future<IReduceCountBits::Result> Filter::countBits(ITexture2DPtr src)
{
    mrMakeFilter(m_count_bits, ReduceCountBits);
    filter->setSrc(src);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}

std::future<IReduceMinMax::Result> Filter::minmax(ITexture2DPtr src)
{
    mrMakeFilter(m_minmax, ReduceMinMax);
    filter->setSrc(src);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}


class Template : public RefCount<ITemplate>
{
public:
    FilterPtr filter;
    ITexture2DPtr tmpl;
    ITexture2DPtr mask;
    ITexture2DPtr result;
    uint32_t mask_bits{};
};
mrConvertile(Template, ITemplate);


class ScreenMatcher : public RefCount<IScreenMatcher>
{
public:
    ScreenMatcher(IGfxInterfacePtr gfx);
    ITemplatePtr createTemplate(const char* path_to_png) override;
    Result match(ITemplatePtr tmpl, HWND target) override;

private:
    IGfxInterfacePtr m_gfx;
    float m_scale = 1.0f;
    int m_contour_block_size = 3;
    int m_expand_block_size = 3;
    float m_binarize_threshold = 0.2f;
};

ScreenMatcher::ScreenMatcher(IGfxInterfacePtr gfx)
    : m_gfx(gfx)
{
}

ITemplatePtr ScreenMatcher::createTemplate(const char* path_to_png)
{
    auto tmpl = m_gfx->createTextureFromFile(path_to_png);
    if (!tmpl)
        return nullptr;

    auto filter = make_ref<Filter>(m_gfx);
    tmpl = filter->transform(tmpl, m_scale, true);
    tmpl = filter->contour(tmpl, m_contour_block_size);
    tmpl = filter->binarize(tmpl, m_binarize_threshold);
    auto mask = filter->expand(tmpl, m_expand_block_size);

    auto ret = make_ref<Template>();
    ret->filter = filter;
    ret->tmpl = tmpl;
    ret->mask = mask;
    ret->mask_bits = filter->countBits(ret->mask).get();
    return ret;
}

IScreenMatcher::Result ScreenMatcher::match(ITemplatePtr tmpl_, HWND target)
{
    auto tmpl = cast(tmpl_);

    ITexture2DPtr screen;
    tmpl->result = tmpl->filter->match(screen, tmpl->tmpl, tmpl->mask);
    return Result();
}

} // namespace mr
