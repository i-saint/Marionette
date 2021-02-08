#include "pch.h"
#include "mrInternal.h"
#include "mrGfxFoundation.h"

#pragma comment(lib, "dwmapi.lib")

namespace mr {

class Template : public RefCount<ITemplate>
{
public:
    void setMatchPattern(MatchPattern v) override { match_pattern = v; }
    ITexture2DPtr getImage() const override { return base_image; }

public:
    MatchPattern match_pattern{};

    // make images for each display resolution scales.
    // (normalizing screen image is too erroneous)

    struct Image
    {
        float scale_factor{}; // corresponding display scale factor
        ITexture2DPtr grayscale{};
        ITexture2DPtr binary{};
        ITexture2DPtr contour{};
        ITexture2DPtr contour_b{};
        ITexture2DPtr mask{};
        uint32_t mask_bits{};
    };
    std::vector<Image> images;
    ITexture2DPtr base_image;

    Image& getImage(float display_scale_factor);
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
        ITexture2DPtr surface;
        ITexture2DPtr grayscale;
        ITexture2DPtr biased;
        ITexture2DPtr binary;
        ITexture2DPtr contour;
        ITexture2DPtr contour_b;
        ITexture2DPtr match_f;
        ITexture2DPtr match_i;
        nanosec last_frame{};
    };

    ScreenMatcher(const Params& params);
    ~ScreenMatcher();
    bool valid() const;

    ITemplatePtr createTemplate(const char* path_to_png) override;

    IReduceMinMaxPtr pullReduceMinmax();
    void pushReduceMinmax(IReduceMinMaxPtr v);

    void updateScreen(ScreenData& sd);
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
static bool g_dbg_sm_writeout = false;

void DbgSetScreenMatcherWriteout(bool v)
{
    g_dbg_sm_writeout = v;
}
#endif // mrDebug

Template::Image& Template::getImage(float display_scale_factor)
{
    for (auto& i : images) {
        if (i.scale_factor == display_scale_factor)
            return i;
    }
    return images.front();
}


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

        int2 size = int2(float2(data.info.rect.size) * m_params.scale);
        data.grayscale  = m_gfx->createTexture(size.x, size.y, TextureFormat::Ru8);
        data.biased     = m_gfx->createTexture(size.x, size.y, TextureFormat::Ru8);
        data.binary     = m_gfx->createTexture(size.x, size.y, TextureFormat::Binary);
        data.contour    = m_gfx->createTexture(size.x, size.y, TextureFormat::Ru8);
        data.contour_b  = m_gfx->createTexture(size.x, size.y, TextureFormat::Binary);
        data.match_f    = m_gfx->createTexture(size.x, size.y, TextureFormat::Rf32);
        data.match_i    = m_gfx->createTexture(size.x, size.y, TextureFormat::Ri32);

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

    auto base_image = m_gfx->createTextureFromFile(path);
    if (!base_image)
        return nullptr;

    auto ret = make_ref<Template>();
    m_templates[path] = ret;
    ret->base_image = base_image;

    auto filter = CreateFilterSet();
    auto create_image = [&](float scale_factor) {
        for (auto& i : ret->images) {
            if (i.scale_factor == scale_factor)
                return; // already created
        }

        int2 size = int2(float2(base_image->getSize()) * m_params.scale * scale_factor);

        Template::Image img{};
        img.scale_factor= scale_factor;
        img.grayscale   = m_gfx->createTexture(size.x, size.y, TextureFormat::Ru8);
        img.binary      = m_gfx->createTexture(size.x, size.y, TextureFormat::Binary);
        img.contour     = m_gfx->createTexture(size.x, size.y, TextureFormat::Ru8);
        img.contour_b   = m_gfx->createTexture(size.x, size.y, TextureFormat::Binary);
        img.mask        = m_gfx->createTexture(size.x, size.y, TextureFormat::Binary);

        filter->grayscale(img.grayscale, base_image, m_params.color_range);
        filter->binarize(img.binary, img.grayscale, m_params.binarize_threshold);

        filter->contour(img.contour, img.grayscale, m_params.contour_radius);
        filter->binarize(img.contour_b, img.contour, m_params.binarize_threshold);
        filter->expand(img.mask, img.contour_b, m_params.expand_radius);
        img.mask_bits = filter->countBits(img.mask).get();

#ifdef mrDebug
        //if (g_dbg_sm_writeout)
        {
            float percent = scale_factor * 100.0f;
            img.grayscale->save(Replace(path, ".png", Format("_grayscale_%.0f.png", percent)));
            img.binary->save(Replace(path, ".png", Format("_binary_%.0f.png", percent)));
            img.contour->save(Replace(path, ".png", Format("_contour_%.0f.png", percent)));
            img.contour_b->save(Replace(path, ".png", Format("_contour_binary_%.0f.png", percent)));
            img.mask->save(Replace(path, ".png", Format("_mask_%.0f.png", percent)));
        }
#endif
        ret->images.push_back(std::move(img));
    };

    if (m_params.care_display_scale) {
        for (auto& kvp : m_screens)
            create_image(kvp.second.info.scale_factor);
    }
    else {
        create_image(1.0f);
    }

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

void ScreenMatcher::updateScreen(ScreenData& sd)
{
    auto frame = sd.capture->getFrame();
    if (!frame.surface)
        return;

    if (frame.present_time != sd.last_frame) {
        // make binarized surface
        sd.last_frame = frame.present_time;
        sd.surface = frame.surface;
        sd.filter->grayscale(sd.grayscale, sd.surface, m_params.color_range);
        sd.filter->binarize(sd.binary, sd.grayscale, m_params.binarize_threshold);

        sd.filter->contour(sd.contour, sd.grayscale, m_params.contour_radius);
        sd.filter->binarize(sd.contour_b, sd.contour, m_params.binarize_threshold);

#ifdef mrDebug
        if (g_dbg_sm_writeout) {
            mrDbgPrint("writing frame %llu\n", sd.last_frame);
            sd.grayscale->save(Format("frame_%llu_grayscale.png", sd.last_frame));
            sd.binary->save(Format("frame_%llu_binary.png", sd.last_frame));
            sd.contour->save(Format("frame_%llu_contour.png", sd.last_frame));
        }
#endif
    }
}

void ScreenMatcher::matchImpl(Template& tmpl, ScreenData& sd, Rect rect)
{
    auto& img = tmpl.getImage(sd.info.scale_factor);

    float scale = m_params.scale;

    auto region = Rect{
        rect.pos - sd.info.rect.pos,
        rect.size
    } * scale;
    region.size -= img.grayscale->getSize();

    if (region.size.x < 0 || region.size.y < 0) {
        // rect is smaller than template. this should not be happened.
        return;
    }

    // dispatch template match & minmax
    auto minmax = pullReduceMinmax();
    minmax->setRegion({ {}, region.size });

    switch (tmpl.match_pattern) {
    case ITemplate::MatchPattern::Grayscale:
        sd.filter->match(sd.match_f, sd.grayscale, img.grayscale, nullptr, region);
        minmax->setSrc(sd.match_f);
        break;
    case ITemplate::MatchPattern::Binary:
        sd.filter->match(sd.match_i, sd.binary, img.binary, nullptr, region);
        minmax->setSrc(sd.match_i);
        break;
    default:
        sd.filter->match(sd.match_i, sd.contour_b, img.contour_b, img.mask, region);
        minmax->setSrc(sd.match_i);
        break;
    }
    minmax->dispatch();

    // make deferred result to dispatch next matching without blocking
    auto deferred = std::async(std::launch::deferred,
        [this, &tmpl, &img, &sd, minmax, scale, rect]() mutable
    {
        auto tsize = img.binary->getSize();
        auto mm = minmax->getResult();
        pushReduceMinmax(minmax);

        Result ret;
        ret.surface = sd.surface;
        ret.region = Rect{
            rect.pos + int2(float2(mm.pos_min) / scale),
            int2(float2(tsize) / scale)
        };
#ifdef mrDebug
        ret.result = sd.match_f;
#endif

        switch (tmpl.match_pattern) {
        case ITemplate::MatchPattern::Grayscale:
            ret.score = float(double(mm.valf_min) / double(tsize.x * tsize.y));
            break;
        case ITemplate::MatchPattern::Binary:
            ret.score = float(double(mm.vali_min) / double(tsize.x * tsize.y));
            break;
        default:
            ret.score = float(double(mm.vali_min) / double(img.mask_bits));
            break;
        }

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
        updateScreen(sd);
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
        updateScreen(sd);
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
