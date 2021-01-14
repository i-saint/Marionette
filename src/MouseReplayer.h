#pragma once

#define mrAPI extern "C" __declspec(dllexport)

namespace mr {

using millisec = uint64_t;
using nanosec = uint64_t;


#ifdef mrDebug
    #define mrEnableProfile
    #define DbgPrint(...) ::mr::Print(__VA_ARGS__)
#else
    //#define mrEnableProfile
    #define DbgPrint(...)
#endif

#ifdef mrEnableProfile
    #define DbgProfile(...) ::mr::ProfileTimer _dbg_pftimer(__VA_ARGS__)
#else
    #define DbgProfile(...)
#endif

void Print(const char* fmt, ...);
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

using IRecorderPtr = std::shared_ptr<IRecorder>;
using IPlayerPtr = std::shared_ptr<IPlayer>;

template<class T>
struct releaser
{
    void operator()(T* p) { p->release(); }
};

inline IRecorderPtr CreateRecorderShared()
{
    return IRecorderPtr(CreateRecorder(), releaser<IRecorder>());
}

inline IPlayerPtr CreatePlayerShared()
{
    return IPlayerPtr(CreatePlayer(), releaser<IPlayer>());
}


// internal

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


class Timer
{
public:
    Timer();
    void reset();
    float elapsed() const; // in sec

private:
    nanosec m_begin = 0;
};

class ProfileTimer : public Timer
{
public:
    ProfileTimer(const char* mes, ...);
    ~ProfileTimer();

private:
    std::string m_message;
};



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

float MatchImage(MatchImageParams& params);
#endif // mrWithOpenCV

} // namespace mr
