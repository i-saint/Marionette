#include "pch.h"
#include "mrInternal.h"
#include "mrGfxFoundation.h"

#pragma comment(lib, "dwmapi.lib")

namespace mr {

class Template : public RefCount<ITemplate>
{
public:
    int2 getSize() const { return size; }
    ITexture2DPtr getImage() const override { return image; }
    ITexture2DPtr getMask() const override { return mask; }
    uint32_t getMaskBits() const override { return mask_bits; }

public:
    int2 size{};
    ITexture2DPtr image{};
    ITexture2DPtr mask{};
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
    ~ScreenMatcher();
    bool valid() const;

    ITemplatePtr createTemplate(const char* path_to_png) override;

    IReduceMinMaxPtr pullReduceMinmax();
    void pushReduceMinmax(IReduceMinMaxPtr v);
    void matchImpl(Template& tmpl, ScreenData& sd, Rect rect);
    Result reduceResults(std::span<ITemplatePtr> tmpl);
    Result match(std::span<ITemplatePtr> tmpl, HMONITOR target) override;
    Result match(std::span<ITemplatePtr> tmpl, HWND target) override;

private:
    // shared with all instances
    struct SharedData : public RefCount<IObject>
    {
        struct ScreenData
        {
            MonitorInfo info;
            IScreenCapturePtr capture;
        };
        std::vector<ScreenData> screens;
    };
    static SharedData* s_data;

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

#ifdef mrDebug
static bool g_dbg_sm_writeout = true;
#endif

ScreenMatcher::SharedData* ScreenMatcher::s_data;

ScreenMatcher::ScreenMatcher(const Params& params)
    : m_gfx(GetGfxInterface())
    , m_params(params)
{
    if (!s_data) {
        s_data = new SharedData();

        // start capture all screens
        EnumerateMonitor([](const MonitorInfo& info) {
            SharedData::ScreenData sd;
            sd.info = info;
            sd.capture = GetGfxInterface()->createScreenCapture();
            if (sd.capture && sd.capture->startCapture(info.hmon))
                s_data->screens.push_back(sd);
            });
    }
    s_data->addRef();

    for (auto& sd : s_data->screens) {
        ScreenData data;
        data.info = sd.info;
        data.capture = sd.capture;
        data.filter = CreateFilterSet();
        m_screens[sd.info.hmon] = std::move(data);
    }
}

ScreenMatcher::~ScreenMatcher()
{
    if (s_data->release() == 0)
        s_data = nullptr;
}

bool ScreenMatcher::valid() const
{
    return !m_screens.empty();
}

ITemplatePtr ScreenMatcher::createTemplate(const char* path)
{
    auto it = m_templates.find(path);
    if (it != m_templates.end())
        return it->second;

    auto tmpl = m_gfx->createTextureFromFile(path);
    if (!tmpl)
        return nullptr;

    auto orig_size = tmpl->getSize();
    auto filter = CreateFilterSet();
    auto trans = filter->transform(tmpl, m_params.scale, true);
    auto contour = filter->contour(trans, m_params.contour_radius);
    auto binalized = filter->binarize(contour, m_params.binarize_threshold);
    auto mask = filter->expand(binalized, m_params.expand_radius);
    tmpl = binalized;

#ifdef mrDebug
    if (g_dbg_sm_writeout) {
        contour->save(Replace(path, ".png", "_contour.png"));
        binalized->save(Replace(path, ".png", "_binalized.png"));
        mask->save(Replace(path, ".png", "_mask.png"));
    }
#endif

    auto ret = make_ref<Template>();
    m_templates[path] = ret;
    ret->size = orig_size;
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
    if (m_params.care_display_scale)
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
        auto contour = sd.filter->contour(surface, m_params.contour_radius);
        sd.binarized = sd.filter->binarize(contour, m_params.binarize_threshold);

#ifdef mrDebug
        if (g_dbg_sm_writeout) {
            surface->save(Format("frame_%llu_trans.png", sd.last_frame));
            sd.binarized->save(Format("frame_%llu_bin.png", sd.last_frame));
        }
#endif
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
