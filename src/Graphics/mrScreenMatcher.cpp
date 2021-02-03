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
    ITexture2DPtr image;
    ITexture2DPtr mask;
    uint32_t mask_bits{};
};
mrConvertile(Template, ITemplate);


class ScreenMatcher : public RefCount<IScreenMatcher>
{
public:
    using DeferredResult = std::future<Result>;
    struct ScreenData
    {
        MonitorInfo info;
        IScreenCapturePtr capture;

        IFilterSetPtr filter;
        ITexture2DPtr binarized;
        nanosec last_frame{};
    };

    ScreenMatcher(const Params& params);
    bool valid() const;

    ITemplatePtr createTemplate(const char* path_to_png) override;

    IReduceMinMaxPtr pullReduceMinmax();
    void pushReduceMinmax(IReduceMinMaxPtr v);
    void matchImpl(Template& tmpl, ScreenData& sd, Rect rect);
    Result reduceResults(std::span<ITemplatePtr> tmpl);
    Result match(std::span<ITemplatePtr> tmpl, HMONITOR target) override;
    Result match(std::span<ITemplatePtr> tmpl, HWND target) override;

private:
    IGfxInterfacePtr m_gfx;
    Params m_params;

    std::map<std::string, ITemplatePtr> m_templates;
    std::map<HMONITOR, ScreenData> m_screens;

    std::deque<IReduceMinMaxPtr> m_reducers;
    std::vector<DeferredResult> m_deferred_results;
};

mrAPI IScreenMatcher* CreateScreenMatcher_(const IScreenMatcher::Params& params)
{
    auto ret = new ScreenMatcher(params);
    if (!ret->valid()) {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

ScreenMatcher::ScreenMatcher(const Params& params)
    : m_gfx(GetGfxInterface())
    , m_params(params)
{
    EnumerateMonitor([this](const MonitorInfo& info) {
        ScreenData data;
        data.info = info;
        data.capture = m_gfx->createScreenCapture();
        if (data.capture && data.capture->startCapture(info.hmon)) {
            data.filter = CreateFilterSet();
            m_screens[info.hmon] = std::move(data);
        }
        });
}

bool ScreenMatcher::valid() const
{
    return m_gfx && !m_screens.empty();
}

ITemplatePtr ScreenMatcher::createTemplate(const char* path)
{
    auto it = m_templates.find(path);
    if (it != m_templates.end())
        return it->second;

    auto tmpl = m_gfx->createTextureFromFile(path);
    if (!tmpl)
        return nullptr;

    auto filter = CreateFilterSet();
    tmpl = filter->transform(tmpl, m_params.scale, true);
    tmpl = filter->contour(tmpl, m_params.contour_block_size);
    tmpl = filter->binarize(tmpl, m_params.binarize_threshold);
    auto mask = filter->expand(tmpl, m_params.expand_block_size);

    auto ret = make_ref<Template>();
    m_templates[path] = ret;
    ret->image = tmpl;
    ret->mask = mask;
    ret->mask_bits = filter->countBits(ret->mask).get();
    return ret;
}

IReduceMinMaxPtr ScreenMatcher::pullReduceMinmax()
{
    IReduceMinMaxPtr ret;
    if (!m_reducers.empty()) {
        ret = m_reducers.front();
        m_reducers.pop_front();
    }
    else {
        ret = m_gfx->createReduceMinMax();
    }
    return ret;
}

void ScreenMatcher::pushReduceMinmax(IReduceMinMaxPtr v)
{
    m_reducers.push_back(v);
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

    if (region.size.x < 0 || region.size.y < 0) {
        // rect is smaller than template
        return;
    }

    auto frame = sd.capture->getFrame();
    if (!frame.surface) {
        return;
    }
    if (frame.present_time != sd.last_frame) {
        // make binarized surface
        sd.last_frame = frame.present_time;
        auto surface = sd.filter->transform(frame.surface, screen_scale, true);
        auto contour = sd.filter->contour(surface, m_params.contour_block_size);
        sd.binarized = sd.filter->binarize(contour, m_params.binarize_threshold);
    }

    // dispatch template match & minmax
    auto match = sd.filter->match(sd.binarized, tmpl.image, tmpl.mask, region, false);

    auto minmax = pullReduceMinmax();
    minmax->setSrc(match);
    minmax->setRegion({ {}, region.size });
    minmax->dispatch();

    // make deferred result to dispatch next matching without blocking
    auto deferred = std::async(std::launch::deferred,
        [this, &tmpl, surface = frame.surface, minmax = std::move(minmax), template_scale, screen_scale, rect]() mutable
    {
        auto mm = minmax->getResult();
        pushReduceMinmax(minmax);

        Result ret;
        ret.surface = surface;
        ret.score = float(double(mm.vali_min) / double(tmpl.mask_bits));
        ret.region = Rect{
            rect.pos + int2(float2(mm.pos_min) / screen_scale),
            int2(float2(tmpl.image->getSize()) / template_scale)
        };
        return ret;
    });
    m_deferred_results.push_back(std::move(deferred));
}

IScreenMatcher::Result ScreenMatcher::reduceResults(std::span<ITemplatePtr> tmpls)
{
    Result ret;
    for (auto& dr : m_deferred_results) {
        auto r = dr.get();
        if (r.score < ret.score)
            ret = r;
    }
    m_deferred_results.clear();
    return ret;
}

IScreenMatcher::Result ScreenMatcher::match(std::span<ITemplatePtr> tmpls, HMONITOR target)
{
    auto i = m_screens.find(target);
    if (i != m_screens.end()) {
        auto& sd = i->second;
        for (auto& t : tmpls)
            matchImpl(cast(*t), sd, sd.info.rect);
    }
    return reduceResults(tmpls);
}

IScreenMatcher::Result ScreenMatcher::match(std::span<ITemplatePtr> tmpls, HWND target)
{
    auto i = m_screens.find(::MonitorFromWindow(target, MONITOR_DEFAULTTONULL));
    if (i != m_screens.end()) {
        auto& sd = i->second;
        auto rect = GetRect(target);
        for (auto& t : tmpls)
            matchImpl(cast(*t), sd, rect);
    }
    return reduceResults(tmpls);
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

mrAPI void WaitVSync()
{
    ::DwmFlush();
}

} // namespace mr
