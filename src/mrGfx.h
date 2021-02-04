#pragma once
#include "mrFoundation.h"

namespace mr {

mrDeclPtr(IGfxInterface);
mrDeclPtr(ITexture2D);
mrDeclPtr(IBuffer);
mrDeclPtr(IScreenCapture);

struct Rect
{
    int2 pos{};
    int2 size{};

    int2 getCenter() const { return pos + (size / 2); }
    int2 getTopRight() const { return pos; }
    int2 getBottomLeft() const { return pos + size; }
    int2 getSize() const { return size; }
    Rect expand(int v) const
    {
        return Rect{ pos - v, size + (v * 2), };
    }

    bool operator==(const Rect& v) const { return pos == v.pos && size == v.size; }
    bool operator!=(const Rect& v) const { return pos != v.pos || size != v.size; }
    Rect operator*(float v) const
    {
        return Rect{
            int2(float2(pos) * v),
            int2(float2(size) * v)
        };
    }
    Rect operator/(float v) const
    {
        return *this * (1.0f / v);
    }
};

enum class TextureFormat
{
    Unknown,
    Ru8,
    RGBAu8,
    BGRAu8,
    Rf32,
    Ri32,
    Binary,
};

class ITexture2D : public IObject
{
public:
    virtual int2 getSize() const = 0;
    virtual TextureFormat getFormat() const = 0;

    using ReadCallback = std::function<void(const void* data, int pitch)>;
    virtual void download() = 0;
    virtual bool map(const ReadCallback& callback) = 0;
    virtual bool read(const ReadCallback& callback) = 0; // download() & map()

    virtual bool save(const std::string& path) = 0;
    virtual std::future<bool> saveAsync(const std::string& path) = 0;
};

class IBuffer : public IObject
{
public:
    virtual int getSize() const = 0;
    virtual int getStride() const = 0;

    using ReadCallback = std::function<void(const void* data)>;
    virtual void download(int size = 0) = 0;
    virtual bool map(const ReadCallback& callback) = 0;
    virtual bool read(const ReadCallback& callback, int size = 0) = 0; // download() & map()
};


class IScreenCapture : public IObject
{
public:
    struct FrameInfo
    {
        ITexture2DPtr surface;
        int2 size{}; // maybe not equal with surface->getSize()
        uint64_t present_time{};
    };
    using Callback = std::function<void(FrameInfo&)>;

    virtual bool startCapture(HWND hwnd) = 0;
    virtual bool startCapture(HMONITOR hmon) = 0;
    virtual void stopCapture() = 0;
    virtual bool isCapturing() const = 0;

    virtual FrameInfo getFrame() = 0;
    virtual FrameInfo waitNextFrame() = 0;
    virtual void setOnFrameArrived(const Callback& cb) = 0;
};


#define mrEachCS(Body)\
    Body(Transform)\
    Body(Normalize)\
    Body(Binarize)\
    Body(Contour)\
    Body(Expand)\
    Body(TemplateMatch)\
    Body(Shape)\
    Body(ReduceTotal)\
    Body(ReduceCountBits)\
    Body(ReduceMinMax)\

#define Body(CS) mrDeclPtr(I##CS)
mrEachCS(Body)
#undef Body


class ICSContext : public IObject
{
public:
    virtual void dispatch() = 0;
};


class IFilter : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual ITexture2DPtr getDst() const = 0;
};

class IReducer : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setRegion(Rect v) = 0;
    virtual int2 getSize() const = 0;
    virtual Rect getRegion() const = 0;
    virtual IBufferPtr getDst() const = 0;
};

class ITransform : public IFilter
{
public:
    virtual void setDstFormat(TextureFormat v) = 0; // ignored if dst is set
    virtual void setSrcRegion(Rect v) = 0;
    virtual void setScale(float v) = 0; // ignored if dst is set
    virtual void setGrayscale(bool v) = 0;
    virtual void setFillAlpha(bool v) = 0;
    virtual void setFiltering(bool v) = 0;
};

class INormalize : public IFilter
{
public:
    virtual void setMax(float v) = 0;
    virtual void setMax(uint32_t v) = 0;
};

class IBinarize : public IFilter
{
public:
    virtual void setThreshold(float v) = 0;
};

class IExpand : public IFilter
{
public:
    virtual void setBlockSize(int v) = 0;
};

class IContour : public IFilter
{
public:
    virtual void setBlockSize(int v) = 0;
};

class ITemplateMatch : public IFilter
{
public:
    virtual void setTemplate(ITexture2DPtr v) = 0;
    virtual void setMask(ITexture2DPtr v) = 0;
    virtual void setRegion(Rect v) = 0;
    virtual void setFitDstSize(bool v) = 0;
};

class IReduceTotal : public IReducer
{
public:
    union Result
    {
        float valf;
        uint32_t vali;
    };
    virtual Result getResult() = 0;
};

class IReduceCountBits : public IReducer
{
public:
    using Result = uint32_t;

    virtual Result getResult() = 0;
};

class IReduceMinMax : public IReducer
{
public:
    struct Result
    {
        int2 pos_min{};
        int2 pos_max{};
        union {
            float valf_min;
            uint32_t vali_min;
        };
        union {
            float valf_max;
            uint32_t vali_max;
        };
        int2 pad;
    };

    virtual Result getResult() = 0;
};

class IShape : public ICSContext
{
public:
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void addCircle(int2 pos, float radius, float border, float4 color) = 0;
    virtual void addRect(Rect rect, float border, float4 color) = 0;
    virtual void clearShapes() = 0;
};


class IGfxInterface : public IObject
{
public:
    virtual ITexture2DPtr createTexture(int w, int h, TextureFormat f, const void* data = nullptr, int pitch = 0) = 0;
    virtual ITexture2DPtr createTextureFromFile(const char* path) = 0;
    virtual IScreenCapturePtr createScreenCapture() = 0;

    // filters
#define Body(CS) virtual I##CS##Ptr create##CS() = 0;
    mrEachCS(Body)
#undef Body

    virtual void flush() = 0;
    virtual void sync(int timeout_ms = 1000) = 0;

    virtual void lock() = 0;
    virtual void unlock() = 0;

    template<class Body>
    void lock(const Body& body)
    {
        std::lock_guard<IGfxInterface> lock(*this);
        body();
    }
};
mrAPI IGfxInterface* GetGfxInterface_();
mrDefShared(GetGfxInterface);


using BitmapCallback = std::function<void(const void* data, int width, int height)>;
mrAPI bool CaptureEntireScreen(const BitmapCallback& callback);
mrAPI bool CaptureScreen(RECT rect, const BitmapCallback& callback);
mrAPI bool CaptureMonitor(HMONITOR hmon, const BitmapCallback& callback);
mrAPI bool CaptureWindow(HWND hwnd, const BitmapCallback& callback);

enum class PixelFormat
{
    Ru8,
    RGBAu8,
    BGRAu8,
};
mrAPI bool SaveAsPNG(const char* path, int w, int h, PixelFormat format, const void* data, int pitch = 0, bool flip_y = false);



// high level API

mrDeclPtr(IFilterSet);
mrDeclPtr(ITemplate);
mrDeclPtr(IScreenMatcher);

class IFilterSet : public IObject
{
public:
    virtual ITexture2DPtr copy(ITexture2DPtr src, Rect src_region, TextureFormat dst_format) = 0;
    inline  ITexture2DPtr copy(ITexture2DPtr src, int2 size, TextureFormat dst_format) { return copy(src, Rect{ {}, size }, dst_format); }
    inline  ITexture2DPtr copy(ITexture2DPtr src, TextureFormat dst_format) { return copy(src, Rect{}, dst_format); }
    inline  ITexture2DPtr copy(ITexture2DPtr src) { return copy(src, Rect{}, TextureFormat::Unknown); }
    virtual ITexture2DPtr transform(ITexture2DPtr src, float scale, bool grayscale, bool filtering, Rect src_region = {}) = 0;
    inline  ITexture2DPtr transform(ITexture2DPtr src, float scale, bool grayscale = false) { return transform(src, scale, grayscale, scale < 1.0f); }
    virtual ITexture2DPtr normalize(ITexture2DPtr src, float denom) = 0;
    virtual ITexture2DPtr binarize(ITexture2DPtr src, float threshold) = 0;
    virtual ITexture2DPtr contour(ITexture2DPtr src, int block_size) = 0;
    virtual ITexture2DPtr expand(ITexture2DPtr src, int block_size) = 0;
    virtual ITexture2DPtr match(ITexture2DPtr src, ITexture2DPtr tmp, ITexture2DPtr mask = nullptr, Rect region = {}, bool fit = true) = 0;

    virtual std::future<IReduceTotal::Result> total(ITexture2DPtr src, Rect region) = 0;
    inline  std::future<IReduceTotal::Result> total(ITexture2DPtr src, int2 region = {}) { return total(src, Rect{ int2{}, region }); }
    virtual std::future<IReduceCountBits::Result> countBits(ITexture2DPtr src, Rect region) = 0;
    inline  std::future<IReduceCountBits::Result> countBits(ITexture2DPtr src, int2 region = {}) { return countBits(src, Rect{ int2{}, region }); }
    virtual std::future<IReduceMinMax::Result> minmax(ITexture2DPtr src, Rect region) = 0;
    inline  std::future<IReduceMinMax::Result> minmax(ITexture2DPtr src, int2 region = {}) { return minmax(src, Rect{ int2{}, region }); }
};
mrAPI IFilterSet* CreateFilterSet_();
inline IFilterSetPtr CreateFilterSet() { return CreateFilterSet_(); }


struct MonitorInfo
{
    HMONITOR hmon{};
    Rect rect{};
    float scale_factor = 1.0f;
};
using MonitorCallback = std::function<void(const MonitorInfo&)>;
mrAPI void EnumerateMonitor(const MonitorCallback& callback);
mrAPI HMONITOR GetPrimaryMonitor();
mrAPI float GetScaleFactor(HMONITOR hmon);
mrAPI Rect ToRect(const RECT& r);
mrAPI Rect GetRect(HWND hwnd);
mrAPI void WaitVSync();

class ITemplate : public IObject
{
public:
    // maybe not equal with getImage()->getSize() because image can be scaled
    virtual int2 getSize() const = 0;
    virtual ITexture2DPtr getImage() const = 0;
    virtual ITexture2DPtr getMask() const = 0;
    virtual uint32_t getMaskBits() const = 0;
};

class IScreenMatcher : public IObject
{
public:
    struct Params
    {
        float scale = 0.5f;
        bool care_display_scale_factor = false;
        int contour_block_size = 3;
        int expand_block_size = 3;
        float binarize_threshold = 0.2f;
    };

    struct Result
    {
        Rect region{};
        float score = 1.0f;
        ITexture2DPtr surface;
    };

    virtual ITemplatePtr createTemplate(const char* path_to_png) = 0;
    virtual Result match(std::span<ITemplatePtr> tmpl, HMONITOR target) = 0;
    virtual Result match(std::span<ITemplatePtr> tmpl, HWND target) = 0;
    inline Result match(ITemplatePtr tmpl, HMONITOR target) { return match(MakeSpan(tmpl), target); }
    inline Result match(ITemplatePtr tmpl, HWND target) { return match(MakeSpan(tmpl), target); }
    inline Result match(std::vector<ITemplatePtr>& tmpl, HMONITOR target) { return match(MakeSpan(tmpl), target); }
    inline Result match(std::vector<ITemplatePtr>& tmpl, HWND target) { return match(MakeSpan(tmpl), target); }
};
mrAPI IScreenMatcher* CreateScreenMatcher_(const IScreenMatcher::Params& params);
inline IScreenMatcherPtr CreateScreenMatcher(const IScreenMatcher::Params& params = {}) { return CreateScreenMatcher_(params); }


#ifdef mrWithOpenCV
struct MatchImageParams
{
    // inputs
    std::vector<cv::Mat> template_images;
    int block_size = 11;
    float color_offset = -10.0;
    bool care_scale_factor = true;
    MatchTarget match_target = MatchTarget::EntireScreen;

    // outputs
    HWND target_window = nullptr;
    float score = 0.0f;
    cv::Point position{};
};
mrAPI float MatchImage(MatchImageParams& params);
cv::Mat MakeCVImage(const void* data, int width, int height, int pitch, int ch = 4, bool flip_y = false);
#endif // mrWithOpenCV

} // namespace mr
