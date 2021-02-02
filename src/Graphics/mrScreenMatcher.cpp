#include "pch.h"
#include "mrInternal.h"
#include "mrGfxFoundation.h"

namespace mr {

class Template : public RefCount<ITemplate>
{
public:
    ITexture2DPtr getImage() const override { return image; }
    ITexture2DPtr getMask() const override { return mask; }
    uint32_t getMaskBits() const override { return mask_bits; }

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

    ScreenMatcher(IGfxInterfacePtr gfx, const Params& params);
    bool valid() const;

    ITemplatePtr createTemplate(const char* path_to_png) override;

    Result matchImpl(Template& tmpl, ScreenData& sd, Rect rect);
    Result match(ITemplatePtr tmpl, HMONITOR target) override;
    Result match(ITemplatePtr tmpl, HWND target) override;

private:
    IGfxInterfacePtr m_gfx;
    Params m_params;

    std::map<HMONITOR, ScreenData> m_screens;
};

mrAPI IScreenMatcher* CreateScreenMatcher_(IGfxInterface* gfx, const IScreenMatcher::Params& params)
{
    auto ret = new ScreenMatcher(gfx, params);
    if (!ret->valid()) {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

ScreenMatcher::ScreenMatcher(IGfxInterfacePtr gfx, const Params& params)
    : m_gfx(gfx)
    , m_params(params)
{
    EnumerateMonitor([this](const MonitorInfo& info) {
        ScreenData data;
        data.info = info;
        data.capture = m_gfx->createScreenCapture();
        if (data.capture && data.capture->startCapture(info.hmon)) {
            data.filter = CreateFilterSet(m_gfx);
            m_screens[info.hmon] = std::move(data);
        }
        });
}

bool ScreenMatcher::valid() const
{
    return m_gfx && !m_screens.empty();
}

ITemplatePtr ScreenMatcher::createTemplate(const char* path_to_png)
{
    auto tmpl = m_gfx->createTextureFromFile(path_to_png);
    if (!tmpl)
        return nullptr;

    auto filter = CreateFilterSet(m_gfx);
    tmpl = filter->transform(tmpl, m_params.scale, true);
    tmpl = filter->contour(tmpl, m_params.contour_block_size);
    tmpl = filter->binarize(tmpl, m_params.binarize_threshold);
    auto mask = filter->expand(tmpl, m_params.expand_block_size);

    auto ret = make_ref<Template>();
    ret->filter = filter;
    ret->image = tmpl;
    ret->mask = mask;
    ret->mask_bits = filter->countBits(ret->mask).get();
    return ret;
}

IScreenMatcher::Result ScreenMatcher::matchImpl(Template& tmpl, ScreenData& sd, Rect rect)
{
    float template_scale = m_params.scale;
    float screen_scale = m_params.scale;
    if (m_params.care_display_scale_factor)
        screen_scale /= sd.info.scale_factor;

    auto region = Rect{
        rect.pos - sd.info.rect.pos,
        rect.size
    } * screen_scale;
    region.size -= tmpl.image->getSize();

    auto frame = sd.capture->getFrame();
    auto surface = sd.filter->transform(frame.surface, screen_scale, true);
    auto contour = sd.filter->contour(surface, m_params.contour_block_size);
    auto binarized = sd.filter->binarize(contour, m_params.binarize_threshold);

    auto match_result = sd.filter->match(binarized, tmpl.image, tmpl.mask, region, false);
    auto minmax = tmpl.filter->minmax(match_result, region.size).get();

    Result ret;
    ret.region = Rect{
        rect.pos + int2(float2(minmax.pos_min) / screen_scale),
        int2(float2(tmpl.image->getSize()) / template_scale)
    };
    ret.score = float(double(minmax.vali_min) / double(tmpl.mask_bits));
    ret.surface = frame.surface;
    ret.match_result = match_result;
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
