#pragma once
#include "Graphics/Vector.h"

#define mrAPI extern "C" __declspec(dllexport)

namespace mr {

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
    virtual ~IRecorder() {}
    virtual void release() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isRecording() const = 0;
    virtual bool update() = 0;
    virtual bool save(const char* path) const = 0;

    virtual void addRecord(const OpRecord& rec) = 0;
};

class IPlayer
{
public:
    virtual ~IPlayer() {}
    virtual void release() = 0;
    virtual bool start(uint32_t loop = 1) = 0;
    virtual bool stop() = 0;
    virtual bool isPlaying() const = 0;
    virtual bool update() = 0;
    virtual bool load(const char* path) = 0;
    virtual void setMatchTarget(MatchTarget v) = 0;
};

mrAPI IRecorder* CreateRecorder();
mrAPI IPlayer* CreatePlayer();


// utils

template<class T>
struct releaser
{
    void operator()(T* p) { p->release(); }
};

#define mrDeclPtr(T) using T##Ptr = std::shared_ptr<T>;

#define mrDefShared(F)\
    inline auto F##Shared()\
    {\
        using T = std::remove_pointer_t<decltype(F())>;\
        return std::shared_ptr<T>(F(), releaser<T>());\
    }


mrDeclPtr(IRecorder);
mrDeclPtr(IPlayer);

mrDefShared(CreateRecorder);
mrDefShared(CreatePlayer);


class IInputReceiver
{
public:
    virtual ~IInputReceiver() {}
    virtual bool valid() const = 0;
    virtual void update() = 0;
    virtual int addHandler(OpRecordHandler v) = 0;
    virtual void removeHandler(int i) = 0;
    virtual int addRecorder(OpRecordHandler v) = 0;
    virtual void removeRecorder(int i) = 0;
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
cv::Mat CaptureScreen(RECT rect);
cv::Mat CaptureEntireScreen();
cv::Mat CaptureWindow(HWND hwnd);
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
    using ReadCallback = std::function<void(const void* data, int pitch)>;

    virtual ~ITexture2D() {};
    virtual void release() = 0;

    virtual int2 getSize() const = 0;
    virtual TextureFormat getFormat() const = 0;

    virtual bool read(const ReadCallback& callback) = 0;
    virtual std::future<bool> readAsync(const ReadCallback& callback) = 0;
};
mrDeclPtr(ITexture2D);


class IScreenCapture
{
public:
    struct FrameInfo
    {
        ITexture2DPtr surface;
        uint64_t present_time{};
        float monitor_scale_factor = 1.0f;
    };

    virtual ~IScreenCapture() {};
    virtual void release() = 0;
    virtual FrameInfo getFrame() const = 0;
};
mrDeclPtr(IScreenCapture);


struct TransformParams
{
    ITexture2DPtr dst; // out. will be created if null
    ITexture2DPtr src;
    int2 offset = int2::zero();
    int2 size = int2::zero();
    float scale = 1.0f;
    bool flip_rb = false;
    bool grayscale = false;
};

struct ContourParams
{
    ITexture2DPtr dst; // out. will be created if null
    ITexture2DPtr src;
    int block_size = 5;
};

struct MatchParams
{
    ITexture2DPtr dst; // out. will be created if null
    ITexture2DPtr src;
    ITexture2DPtr template_image;
};

struct ReduceMinmaxResult
{
    int2 pos_min{};
    int2 pos_max{};
    float val_min{};
    float val_max{};
};

struct ReduceMinmaxParams
{
    std::future<ReduceMinmaxResult> result; // out
    ITexture2DPtr src;
};

class IGfxInterface
{
public:
    using CaptureCallback = std::function<void(ITexture2DPtr tex)>;

    virtual ~IGfxInterface() {};
    virtual void release() = 0;

    virtual ITexture2DPtr createTexture(int w, int h, TextureFormat f, const void* data = nullptr, int pitch = 0) = 0;
    virtual IScreenCapturePtr createScreenCapture(HWND hwnd) = 0;
    virtual IScreenCapturePtr createScreenCapture(HMONITOR hmon) = 0;

    virtual void transform(TransformParams& v) = 0;
    virtual void contour(ContourParams& v) = 0;
    virtual void match(MatchParams& v) = 0;
    virtual void reduceMinMax(ReduceMinmaxParams& v) = 0;

    virtual void flush() = 0;
    virtual void wait() = 0;
};


mrAPI void Initialize();
mrAPI void Finalize();

class InitializeScope
{
public:
    InitializeScope() { ::mr::Initialize(); }
    ~InitializeScope() { ::mr::Finalize(); }
};

} // namespace mr
