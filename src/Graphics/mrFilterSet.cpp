#include "pch.h"
#include "mrInternal.h"
#include "mrGfxFoundation.h"

namespace mr {

class FilterSet : public RefCount<IFilterSet>
{
public:
    FilterSet();

    void copy(ITexture2DPtr dst, ITexture2DPtr src, Rect src_region) override;
    void transform(ITexture2DPtr dst, ITexture2DPtr src, bool grayscale, bool filtering, Rect src_region) override;
    void bias(ITexture2DPtr dst, ITexture2DPtr src, float bias) override;
    void normalize(ITexture2DPtr dst, ITexture2DPtr src, float denom) override;
    void binarize(ITexture2DPtr dst, ITexture2DPtr src, float threshold) override;
    void contour(ITexture2DPtr dst, ITexture2DPtr src, float radius) override;
    void expand(ITexture2DPtr dst, ITexture2DPtr src, float radius) override;
    void match(ITexture2DPtr dst, ITexture2DPtr src, ITexture2DPtr tmp, ITexture2DPtr mask, Rect region) override;

    std::future<IReduceTotal::Result> total(ITexture2DPtr src, Rect region) override;
    std::future<IReduceCountBits::Result> countBits(ITexture2DPtr src, Rect region) override;
    std::future<IReduceMinMax::Result> minmax(ITexture2DPtr src, Rect region) override;

public:
    IGfxInterfacePtr m_gfx;

    ITransformPtr m_copy;
    ITransformPtr m_transform;
    IBiasPtr m_bias;
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


void FilterSet::copy(ITexture2DPtr dst, ITexture2DPtr src, Rect src_region)
{
    mrMakeFilter(m_copy, Transform);
    filter->setDst(dst);
    filter->setSrc(src);
    filter->setSrcRegion(src_region);
    filter->dispatch();
}

void FilterSet::transform(ITexture2DPtr dst, ITexture2DPtr src, bool grayscale, bool filtering, Rect src_region)
{
    mrMakeFilter(m_transform, Transform);
    filter->setDst(dst);
    filter->setSrc(src);
    filter->setSrcRegion(src_region);
    filter->setGrayscale(grayscale);
    filter->setFiltering(filtering);
    filter->dispatch();
}

void FilterSet::bias(ITexture2DPtr dst, ITexture2DPtr src, float bias)
{
    mrMakeFilter(m_bias, Bias);
    filter->setDst(dst);
    filter->setSrc(src);
    filter->setBias(bias);
    filter->dispatch();
}

void FilterSet::normalize(ITexture2DPtr dst, ITexture2DPtr src, float denom)
{
    mrMakeFilter(m_normalize, Normalize);
    filter->setDst(dst);
    filter->setSrc(src);
    filter->setMax(denom);
    filter->dispatch();
}

void FilterSet::binarize(ITexture2DPtr dst, ITexture2DPtr src, float threshold)
{
    mrMakeFilter(m_binarize, Binarize);
    filter->setDst(dst);
    filter->setSrc(src);
    filter->setThreshold(threshold);
    filter->dispatch();
}

void FilterSet::contour(ITexture2DPtr dst, ITexture2DPtr src, float radius)
{
    mrMakeFilter(m_contour, Contour);
    filter->setDst(dst);
    filter->setSrc(src);
    filter->setRadius(radius);
    filter->dispatch();
}

void FilterSet::expand(ITexture2DPtr dst, ITexture2DPtr src, float radius)
{
    mrMakeFilter(m_expand, Expand);
    filter->setDst(dst);
    filter->setSrc(src);
    filter->setRadius(radius);
    filter->dispatch();
}

void FilterSet::match(ITexture2DPtr dst, ITexture2DPtr src, ITexture2DPtr tmp, ITexture2DPtr mask, Rect region)
{
    mrMakeFilter(m_match, TemplateMatch);
    filter->setDst(dst);
    filter->setSrc(src);
    filter->setTemplate(tmp);
    filter->setMask(mask);
    filter->setRegion(region);
    filter->dispatch();
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
