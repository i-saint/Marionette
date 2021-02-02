#include "pch.h"
#include "mrInternal.h"
#include "mrGfxFoundation.h"

namespace mr {

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
