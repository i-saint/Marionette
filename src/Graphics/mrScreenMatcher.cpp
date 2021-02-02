#include "pch.h"
#include "mrInternal.h"
#include "mrGfxFoundation.h"

namespace mr {

class FilterSet : public RefCount<IFilterSet>
{
public:
    FilterSet(IGfxInterfacePtr gfx);

    ITexture2DPtr copy(ITexture2DPtr src, Rect src_region, TextureFormat dst_format) override;
    ITexture2DPtr transform(ITexture2DPtr src, float scale, bool grayscale, bool filtering, Rect src_region) override;
    ITexture2DPtr normalize(ITexture2DPtr src, float denom) override;
    ITexture2DPtr binarize(ITexture2DPtr src, float threshold) override;
    ITexture2DPtr contour(ITexture2DPtr src, int block_size) override;
    ITexture2DPtr expand(ITexture2DPtr src, int block_size) override;
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

mrAPI IFilterSet* CreateFilterSet_(IGfxInterface* gfx)
{
    return new FilterSet(gfx);
}

FilterSet::FilterSet(IGfxInterfacePtr gfx)
    : m_gfx(gfx)
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

ITexture2DPtr FilterSet::contour(ITexture2DPtr src, int block_size)
{
    mrMakeFilter(m_contour, Contour);
    filter->setSrc(src);
    filter->setBlockSize(block_size);
    filter->dispatch();
    return filter->getDst();
}

ITexture2DPtr FilterSet::expand(ITexture2DPtr src, int block_size)
{
    mrMakeFilter(m_expand, Expand);
    filter->setSrc(src);
    filter->setBlockSize(block_size);
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


class Template : public RefCount<ITemplate>
{
public:
    IFilterSetPtr filter;
    ITexture2DPtr image;
    ITexture2DPtr mask;
    uint32_t mask_bits{};
};
mrConvertile(Template, ITemplate);


class ScreenMatcher : public RefCount<IScreenMatcher>
{
public:
    struct ScreenData
    {
        MonitorInfo info;
        IScreenCapturePtr capture;
        nanosec last_frame{};
        IFilterSetPtr filter;
    };

    ScreenMatcher(IGfxInterfacePtr gfx);
    ITemplatePtr createTemplate(const char* path_to_png) override;

    Result matchImpl(Template& tmpl, ScreenData& sd, Rect rect);
    Result match(ITemplatePtr tmpl, HMONITOR target) override;
    Result match(ITemplatePtr tmpl, HWND target) override;

private:
    IGfxInterfacePtr m_gfx;
    float m_scale = 1.0f;
    int m_contour_block_size = 3;
    int m_expand_block_size = 3;
    float m_binarize_threshold = 0.2f;

    std::map<HMONITOR, ScreenData> m_screens;
};

mrAPI IScreenMatcher* CreateScreenMatcher_(IGfxInterface* gfx)
{
    return new ScreenMatcher(gfx);
}

ScreenMatcher::ScreenMatcher(IGfxInterfacePtr gfx)
    : m_gfx(gfx)
{
    EnumerateMonitor([this](const MonitorInfo& info) {
        ScreenData data;
        data.info = info;
        data.filter = CreateFilterSet(m_gfx);
        data.capture = m_gfx->createScreenCapture();
        data.capture->startCapture(info.hmon);
        m_screens[info.hmon] = std::move(data);
        });
}

ITemplatePtr ScreenMatcher::createTemplate(const char* path_to_png)
{
    auto tmpl = m_gfx->createTextureFromFile(path_to_png);
    if (!tmpl)
        return nullptr;

    auto filter = CreateFilterSet(m_gfx);
    tmpl = filter->transform(tmpl, m_scale, true);
    tmpl = filter->contour(tmpl, m_contour_block_size);
    tmpl = filter->binarize(tmpl, m_binarize_threshold);
    auto mask = filter->expand(tmpl, m_expand_block_size);

    auto ret = make_ref<Template>();
    ret->filter = filter;
    ret->image = tmpl;
    ret->mask = mask;
    ret->mask_bits = filter->countBits(ret->mask).get();
    return ret;
}

IScreenMatcher::Result ScreenMatcher::matchImpl(Template& tmpl, ScreenData& sd, Rect rect)
{
    // todo: care screen->info.scale_factor

    Rect region = {
        rect.pos - sd.info.rect.pos,
        rect.size - tmpl.image->getSize()
    };

    auto frame = sd.capture->getFrame();
    auto surface = sd.filter->transform(frame.surface, m_scale, true);

    auto score = sd.filter->match(surface, tmpl.image, tmpl.mask, region, false);
    auto result = tmpl.filter->minmax(score, region).get();

    // todo: care scale and offset
    Result ret;
    ret.pos = result.pos_min;
    ret.size = tmpl.image->getSize();
    ret.score = result.valf_min;
    return ret;
}

IScreenMatcher::Result ScreenMatcher::match(ITemplatePtr tmpl_, HMONITOR target)
{
    auto tmpl = cast(tmpl_);

    auto i = m_screens.find(target);
    if (i != m_screens.end()) {
        auto& sd = i->second;
        return matchImpl(*tmpl, sd, sd.info.rect);
    }

    Result ret;
    ret.score = 1.0f; // zero matching
    return ret;
}

IScreenMatcher::Result ScreenMatcher::match(ITemplatePtr tmpl_, HWND target)
{
    auto tmpl = cast(tmpl_);

    auto i = m_screens.find(::MonitorFromWindow(target, MONITOR_DEFAULTTONULL));
    if (i != m_screens.end()) {
        return matchImpl(*tmpl, i->second, GetRect(target));
    }

    Result ret;
    ret.score = 1.0f; // zero matching
    return ret;
}


static BOOL EnumerateMonitorCB(HMONITOR hmon, HDC hdc, LPRECT rect, LPARAM userdata)
{
    MonitorInfo sinfo{};
    sinfo.hmon = hmon;
    sinfo.scale_factor = GetScaleFactor(hmon);
    sinfo.rect = ToRect(*rect);

    auto callback = (const MonitorCallback*)userdata;
    (*callback)(sinfo);

    return TRUE;
}

mrAPI void EnumerateMonitor(const MonitorCallback& callback)
{
    ::EnumDisplayMonitors(nullptr, nullptr, EnumerateMonitorCB, (LPARAM)&callback);
}

mrAPI HMONITOR GetPrimaryMonitor()
{
    return ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
}

mrAPI float GetScaleFactor(HMONITOR hmon)
{
    UINT dpix, dpiy;
    ::GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);
    return (float)dpix / 96.0;
}

mrAPI Rect ToRect(const RECT& r)
{
    auto tl = int2{ r.left, r.top };
    auto br = int2{ r.right, r.bottom };
    return Rect{ tl, br - tl, };
}

mrAPI Rect GetRect(HWND hwnd)
{
    RECT wr;
    ::GetWindowRect(hwnd, &wr);
    return ToRect(wr);
}

} // namespace mr
