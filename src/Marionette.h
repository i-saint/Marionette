#pragma once
#include "Graphics/Vector.h"
#include "ref_ptr.h"

#define mrAPI extern "C" __declspec(dllexport)

namespace mr {

#define mrDeclPtr(T)\
    class T;\
    using T##Ptr = ref_ptr<T>;

#define mrDefShared(F)\
    inline auto F##()\
    {\
        using T = std::remove_pointer_t<decltype(F##_())>;\
        return ref_ptr<T>(F##_());\
    }



using millisec = uint64_t;
using nanosec = uint64_t;

#ifdef mrDebug
    #define mrEnableProfile
    #define mrDbgPrint(...) ::mr::Print(__VA_ARGS__)
#else
    //#define mrEnableProfile
    #define mrDbgPrint(...)
#endif

#ifdef mrEnableProfile
    #define mrProfile(...) ::mr::ProfileTimer _dbg_pftimer(__VA_ARGS__)
#else
    #define mrProfile(...)
#endif

void Print(const char* fmt, ...);
void Print(const wchar_t* fmt, ...);
millisec NowMS();
nanosec NowNS();
void SleepMS(millisec v);
std::string GetCurrentModuleDirectory();


enum class OpType : int
{
    Unknown,
    Wait,
    KeyDown,
    KeyUp,
    MouseDown,
    MouseUp,
    MouseMoveAbs,
    MouseMoveRel,
    MouseMoveMatch,
    SaveMousePos,
    LoadMousePos,
};

struct OpRecord
{
    struct ImageData
    {
        int handle = 0;
        std::string path;
    };

    OpType type = OpType::Unknown;
    millisec time = -1;
    union
    {
        struct
        {
            int x, y, button;
        } mouse;
        struct
        {
            int code;
        } key;
    } data{};

    struct
    {
        int save_slot = 0;
        std::vector<ImageData> images;
    } exdata;

    std::string toText() const;
    bool fromText(const std::string& v);
};
using OpRecordHandler = std::function<bool (OpRecord& rec)>;

enum class MatchTarget
{
    EntireScreen,
    ForegroundWindow,
};


mrDeclPtr(IRecorder);
mrDeclPtr(IPlayer);

mrDeclPtr(IGfxInterface);
mrDeclPtr(ITexture2D);
mrDeclPtr(IBuffer);
mrDeclPtr(IScreenCapture);


class IObject
{
public:
    virtual int addRef() = 0;
    virtual int release() = 0;
    virtual int getRef() const = 0;

protected:
    virtual ~IObject() {}
};

class IRecorder : public IObject
{
public:
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isRecording() const = 0;
    virtual bool update() = 0;
    virtual bool save(const char* path) const = 0;

    virtual void addRecord(const OpRecord& rec) = 0;
};
mrAPI IRecorder* CreateRecorder_();
mrDefShared(CreateRecorder);


class IPlayer : public IObject
{
public:
    virtual bool start(uint32_t loop = 1) = 0;
    virtual bool stop() = 0;
    virtual bool isPlaying() const = 0;
    virtual bool update() = 0;
    virtual bool load(const char* path) = 0;
    virtual void setMatchTarget(MatchTarget v) = 0;
};
mrAPI IPlayer* CreatePlayer_();
mrDefShared(CreatePlayer);


class IInputReceiver
{
public:
    virtual bool valid() const = 0;
    virtual void update() = 0;
    virtual int addHandler(OpRecordHandler v) = 0;
    virtual void removeHandler(int i) = 0;
    virtual int addRecorder(OpRecordHandler v) = 0;
    virtual void removeRecorder(int i) = 0;

protected:
    virtual ~IInputReceiver() {}
};
mrAPI IInputReceiver* GetReceiver();

struct Key
{
    uint32_t ctrl : 1;
    uint32_t alt : 1;
    uint32_t shift : 1;
    uint32_t code : 29;
};
inline bool operator<(const Key& a, const Key& b) { return (uint32_t&)a < (uint32_t&)b; }
inline bool operator>(const Key& a, const Key& b) { return (uint32_t&)a > (uint32_t&)b; }
inline bool operator==(const Key& a, const Key& b) { return (uint32_t&)a == (uint32_t&)b; }

std::map<Key, std::string> LoadKeymap(const char* path, const std::function<void(Key key, std::string path)>& body);
void Split(const std::string& str, const std::string& separator, const std::function<void(std::string sub)>& body);
void Scan(const std::string& str, const std::regex& exp, const std::function<void(std::string sub)>& body);



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
        uint64_t present_time{};
    };
    using Callback = std::function<void(FrameInfo&)>;

    virtual bool valid() const = 0;
    virtual bool startCapture(HWND hwnd) = 0;
    virtual bool startCapture(HMONITOR hmon) = 0;
    virtual void stopCapture() = 0;
    virtual FrameInfo getFrame() = 0;
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

class ITransform : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setDstFormat(TextureFormat v) = 0; // ignored if dst is set
    virtual void setRect(int2 pos, int2 size) = 0;
    virtual void setScale(float v) = 0; // ignored if dst is set
    virtual void setGrayscale(bool v) = 0;
    virtual void setFillAlpha(bool v) = 0;
    virtual void setFiltering(bool v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};

class INormalize : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setMax(float v) = 0;
    virtual void setMax(uint32_t v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};

class IBinarize : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setThreshold(float v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};

class IExpand : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setSize(int v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};

class IContour : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setBlockSize(int v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};

class ITemplateMatch : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setTemplate(ITexture2DPtr v) = 0;
    virtual void setMask(ITexture2DPtr v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};

class IShape : public ICSContext
{
public:
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void addCircle(int2 pos, float radius, float border, float4 color) = 0;
    virtual void addRect(int2 pos, int2 size, float border, float4 color) = 0;
    virtual void clearShapes() = 0;
};

class IReduceTotal : public ICSContext
{
public:
    union Result
    {
        float valf;
        uint32_t vali;
    };
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual Result getResult() = 0;
};

class IReduceCountBits : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual uint32_t getResult() = 0;
};

class IReduceMinMax : public ICSContext
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

    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual Result getResult() = 0;
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
mrAPI IGfxInterface* CreateGfxInterface_();
mrDefShared(CreateGfxInterface);


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


mrAPI void Initialize();
mrAPI void Finalize();

class InitializeScope
{
public:
    InitializeScope() { ::mr::Initialize(); }
    ~InitializeScope() { ::mr::Finalize(); }
};



mrAPI HMONITOR GetPrimaryMonitor();
mrAPI float GetScaleFactor(HMONITOR hmon);

} // namespace mr
