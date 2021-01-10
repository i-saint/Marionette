#pragma once

#define mrAPI extern "C" __declspec(dllexport)

namespace mr {

using millisec = uint64_t;

#ifdef mrDebug
    #define DbgPrint(...) ::mr::Print(__VA_ARGS__)
#else
    #define DbgPrint(...)
#endif
void Print(const char* fmt, ...);
millisec NowMS();
void SleepMS(millisec v);


enum class OpType : int
{
    Unknown,
    Wait,
    PushState,
    PopState,
    KeyDown,
    KeyUp,
    MouseDown,
    MouseUp,
    MouseMoveAbs,
    MouseMoveRel,
    MouseMoveMatch,
};

struct OpRecord
{
    OpType type = OpType::Unknown;
    millisec time = -1;
    struct
    {
        struct
        {
            int x, y, button;
            int image_handle;
            std::string image_path;
        } mouse;
        struct
        {
            int code;
        } key;
    } data{};

    std::string toText() const;
    bool fromText(const std::string& v);
};

using OpRecordHandler = std::function<bool (OpRecord& rec)>;


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


#ifdef mrWithOpenCV
std::tuple<bool, int, int> MatchImage(HWND hwnd, const cv::Mat& tmp_img);
#endif // mrWithOpenCV

} // namespace mr
