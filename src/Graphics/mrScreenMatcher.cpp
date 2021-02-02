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

    std::future<IScreenMatcher::Result> deferred_result;
};
mrConvertile(Template, ITemplate);


class ScreenMatcher : public RefCount<IScreenMatcher>
{
public:
    struct ScreenData
    {
        MonitorInfo info;
        IScreenCapturePtr capture;

        IFilterSetPtr filter;
        ITexture2DPtr binarized;
        nanosec last_frame{};
    };

    ScreenMatcher(IGfxInterfacePtr gfx, const Params& params);
    bool valid() const;

    ITemplatePtr createTemplate(const char* path_to_png) override;

    void matchImpl(Template& tmpl, ScreenData& sd, Rect rect);
    Result match(std::span<ITemplatePtr> tmpl, HMONITOR target) override;
    Result match(std::span<ITemplatePtr> tmpl, HWND target) override;

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

void ScreenMatcher::matchImpl(Template& tmpl, ScreenData& sd, Rect rect)
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
    // make binarized surface
    if (frame.present_time != sd.last_frame) {
        sd.last_frame = frame.present_time;
        auto surface = sd.filter->transform(frame.surface, screen_scale, true);
        auto contour = sd.filter->contour(surface, m_params.contour_block_size);
        sd.binarized = sd.filter->binarize(contour, m_params.binarize_threshold);
    }

    // dispatch template match & minmax
    auto match = sd.filter->match(sd.binarized, tmpl.image, tmpl.mask, region, false);
    auto minmax_f = tmpl.filter->minmax(match, region.size); // tmpl.filter to store result in tmpl

    // make deferred result to dispatch next matching without blocking
    Result ret;
    ret.surface = frame.surface;
    ret.match_result = match;
    tmpl.deferred_result = std::async(std::launch::deferred,
        [&tmpl, ret = std::move(ret), minmax_f = std::move(minmax_f), template_scale, screen_scale, rect]() mutable
    {
        auto minmax = minmax_f.get();
        ret.score = float(double(minmax.vali_min) / double(tmpl.mask_bits));
        ret.region = Rect{
            rect.pos + int2(float2(minmax.pos_min) / screen_scale),
            int2(float2(tmpl.image->getSize()) / template_scale)
        };
        return ret;
    });
}

IScreenMatcher::Result ScreenMatcher::match(std::span<ITemplatePtr> tmpls, HMONITOR target)
{
    Result ret;

    auto i = m_screens.find(target);
    if (i != m_screens.end()) {
        auto& sd = i->second;
        for (auto& t : tmpls) {
            matchImpl(cast(*t), sd, sd.info.rect);
        }
        for (auto& t : tmpls) {
            auto r = cast(*t).deferred_result.get();
            if (r.score < ret.score)
                ret = r;
        }
    }

    return ret;
}

IScreenMatcher::Result ScreenMatcher::match(std::span<ITemplatePtr> tmpls, HWND target)
{
    Result ret;

    auto i = m_screens.find(::MonitorFromWindow(target, MONITOR_DEFAULTTONULL));
    if (i != m_screens.end()) {
        auto& sd = i->second;
        auto rect = GetRect(target);
        for (auto& t : tmpls) {
            matchImpl(cast(*t), sd, rect);
        }
        for (auto& t : tmpls) {
            auto r = cast(*t).deferred_result.get();
            if (r.score < ret.score)
                ret = r;
        }
    }

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
