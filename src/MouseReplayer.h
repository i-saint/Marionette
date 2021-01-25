#pragma once
#include "Graphics/Vector.h"

#define mrAPI extern "C" __declspec(dllexport)

namespace mr {

template<class T>
struct releaser
{
    void operator()(T* p) { p->release(); }
};

#define mrDeclPtr(T)\
     using T##Ptr = std::shared_ptr<T>;

#define mrDefShared(F)\
    inline auto F##()\
    {\
        using T = std::remove_pointer_t<decltype(F##_())>;\
        return std::shared_ptr<T>(F##_(), releaser<T>());\
    }

#define mrMkPtr(...)\
    using T = std::remove_pointer_t<decltype(__VA_ARGS__)>;\
    return std::shared_ptr<T>(__VA_ARGS__, releaser<T>())


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



class IRecorder
{
public:
    virtual void release() = 0;

    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isRecording() const = 0;
    virtual bool update() = 0;
    virtual bool save(const char* path) const = 0;

    virtual void addRecord(const OpRecord& rec) = 0;

protected:
    virtual ~IRecorder() {}
};
mrDeclPtr(IRecorder);
mrAPI IRecorder* CreateRecorder_();
mrDefShared(CreateRecorder);

class IPlayer
{
public:
    virtual void release() = 0;

    virtual bool start(uint32_t loop = 1) = 0;
    virtual bool stop() = 0;
    virtual bool isPlaying() const = 0;
    virtual bool update() = 0;
    virtual bool load(const char* path) = 0;
    virtual void setMatchTarget(MatchTarget v) = 0;

protected:
    virtual ~IPlayer() {}
};
mrDeclPtr(IPlayer);
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
    Rf32,
    Ri32,
};

class ITexture2D
{
public:
    virtual void release() = 0;

    virtual int2 getSize() const = 0;
    virtual TextureFormat getFormat() const = 0;

    using ReadCallback = std::function<void(const void* data, int pitch)>;
    virtual void download() = 0;
    virtual bool map(const ReadCallback& callback) = 0;
    virtual bool read(const ReadCallback& callback) = 0; // download() & map()

    virtual bool save(const std::string& path) = 0;
    virtual std::future<bool> saveAsync(const std::string& path) = 0;

protected:
    virtual ~ITexture2D() {};
};
mrDeclPtr(ITexture2D);


class IScreenCapture
{
public:
    struct FrameInfo
    {
        ITexture2DPtr surface;
        uint64_t present_time{};
    };
    using Callback = std::function<void(FrameInfo&)>;

    virtual void release() = 0;
    virtual bool valid() const = 0;
    virtual bool startCapture(HWND hwnd) = 0;
    virtual bool startCapture(HMONITOR hmon) = 0;
    virtual void stopCapture() = 0;
    virtual FrameInfo getFrame() = 0;
    virtual void setOnFrameArrived(const Callback& cb) = 0;

protected:
    virtual ~IScreenCapture() {};
};
mrDeclPtr(IScreenCapture);


class ICSContext
{
public:
    virtual void release() = 0;
    virtual void dispatch() = 0;

protected:
    virtual ~ICSContext() {}
};

class ITransform : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setRect(int2 pos, int2 size) = 0;
    virtual void setScale(float v) = 0;
    virtual void setGrayscale(bool v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};
mrDeclPtr(ITransform);

class IBinarize : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setThreshold(float v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};
mrDeclPtr(IBinarize);

class IContour : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setBlockSize(int v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};
mrDeclPtr(IContour);

class ITemplateMatch : public ICSContext
{
public:
    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual void setDst(ITexture2DPtr v) = 0;
    virtual void setTemplate(ITexture2DPtr v) = 0;
    virtual ITexture2DPtr getDst() = 0;
};
mrDeclPtr(ITemplateMatch);

class IReduceMinMax : public ICSContext
{
public:
    struct Result
    {
        int2 pos_min{};
        int2 pos_max{};
        float val_min{};
        float val_max{};
        int2 pad;
    };

    virtual void setSrc(ITexture2DPtr v) = 0;
    virtual Result getResult() = 0;
};
mrDeclPtr(IReduceMinMax);


class IGfxInterface
{
public:
    virtual void release() = 0;

    ITexture2DPtr createTexture(int w, int h, TextureFormat f, const void* data = nullptr, int pitch = 0) { mrMkPtr(createTexture_(w, h, f, data, pitch)); }
    ITexture2DPtr createTextureFromFile(const char* path) { mrMkPtr(createTextureFromFile_(path)); }
    IScreenCapturePtr createScreenCapture() { mrMkPtr(createScreenCapture_()); }

    ITransformPtr createTransform() { mrMkPtr(createTransform_()); }
    IBinarizePtr createBinarize() { mrMkPtr(createBinarize_()); }
    IContourPtr createContour() { mrMkPtr(createContour_()); }
    ITemplateMatchPtr createTemplateMatch() { mrMkPtr(createTemplateMatch_()); }
    IReduceMinMaxPtr createReduceMinMax() { mrMkPtr(createReduceMinMax_()); }

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

protected:
    virtual ~IGfxInterface() {};

    virtual ITexture2D* createTexture_(int w, int h, TextureFormat f, const void* data, int pitch) = 0;
    virtual ITexture2D* createTextureFromFile_(const char* path) = 0;
    virtual IScreenCapture* createScreenCapture_() = 0;

    virtual ITransform* createTransform_() = 0;
    virtual IBinarize* createBinarize_() = 0;
    virtual IContour* createContour_() = 0;
    virtual ITemplateMatch* createTemplateMatch_() = 0;
    virtual IReduceMinMax* createReduceMinMax_() = 0;
};
mrDeclPtr(IGfxInterface);
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
