#include "pch.h"
#include "mrInternal.h"
#include "mrGfxFoundation.h"

namespace mr {

class FilterSet : public RefCount<IFilterSet>
{
public:
    FilterSet();

    ITexture2DPtr copy(ITexture2DPtr src, Rect src_region, TextureFormat dst_format) override;
    ITexture2DPtr transform(ITexture2DPtr src, float scale, bool grayscale, bool filtering, Rect src_region) override;
    ITexture2DPtr normalize(ITexture2DPtr src, float denom) override;
    ITexture2DPtr binarize(ITexture2DPtr src, float threshold) override;
    ITexture2DPtr contour(ITexture2DPtr src, float radius) override;
    ITexture2DPtr expand(ITexture2DPtr src, float radius) override;
    ITexture2DPtr match(ITexture2DPtr src, ITexture2DPtr tmp, ITexture2DPtr mask, Rect region, bool fit) override;

    std::future<IReduceTotal::Result> total(ITexture2DPtr src, Rect region) override;
    std::future<IReduceCountBits::Result> countBits(ITexture2DPtr src, Rect region) override;
    std::future<IReduceMinMax::Result> minmax(ITexture2DPtr src, Rect region) override;

public:
    IGfxInterfacePtr m_gfx;

    ITransformPtr m_copy;
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
mrDeclPtr(FilterSet);

mrAPI IFilterSet* CreateFilterSet_()
{
    return new FilterSet();
}

FilterSet::FilterSet()
    : m_gfx(GetGfxInterface())
{
}
#define mrMakeFilter(N, T)\
    if (!N)\
        N = m_gfx->create##T();\
    auto& filter = N;


ITexture2DPtr FilterSet::copy(ITexture2DPtr src, Rect src_region, TextureFormat dst_format)
{
    mrMakeFilter(m_copy, Transform);
    filter->setSrc(src);
    filter->setSrcRegion(src_region);
    filter->setDstFormat(dst_format);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr FilterSet::transform(ITexture2DPtr src, float scale, bool grayscale, bool filtering, Rect src_region)
{
    mrMakeFilter(m_transform, Transform);
    filter->setSrc(src);
    filter->setScale(scale);
    filter->setSrcRegion(src_region);
    filter->setGrayscale(grayscale);
    filter->setFiltering(filtering);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr FilterSet::normalize(ITexture2DPtr src, float denom)
{
    mrMakeFilter(m_normalize, Normalize);
    filter->setSrc(src);
    filter->setMax(denom);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr FilterSet::binarize(ITexture2DPtr src, float threshold)
{
    mrMakeFilter(m_binarize, Binarize);
    filter->setSrc(src);
    filter->setThreshold(threshold);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr FilterSet::contour(ITexture2DPtr src, float radius)
{
    mrMakeFilter(m_contour, Contour);
    filter->setSrc(src);
    filter->setRadius(radius);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr FilterSet::expand(ITexture2DPtr src, float radius)
{
    mrMakeFilter(m_expand, Expand);
    filter->setSrc(src);
    filter->setRadius(radius);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr FilterSet::match(ITexture2DPtr src, ITexture2DPtr tmp, ITexture2DPtr mask, Rect region, bool fit)
{
    mrMakeFilter(m_match, TemplateMatch);
    filter->setSrc(src);
    filter->setTemplate(tmp);
    filter->setMask(mask);
    filter->setRegion(region);
    filter->setFitDstSize(fit);
    filter->dispatch();
    return filter->getDst();
}


std::future<IReduceTotal::Result> FilterSet::total(ITexture2DPtr src, Rect region)
{
    mrMakeFilter(m_total, ReduceTotal);
    filter->setSrc(src);
    filter->setRegion(region);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}

std::future<IReduceCountBits::Result> FilterSet::countBits(ITexture2DPtr src, Rect region)
{
    mrMakeFilter(m_count_bits, ReduceCountBits);
    filter->setSrc(src);
    filter->setRegion(region);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}

std::future<IReduceMinMax::Result> FilterSet::minmax(ITexture2DPtr src, Rect region)
{
    mrMakeFilter(m_minmax, ReduceMinMax);
    filter->setSrc(src);
    filter->setRegion(region);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}

} // namespace mr
