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
    MouseMove,
    MouseDown,
    MouseUp,
    KeyDown,
    KeyUp,
};

struct OpRecord
{
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

    std::string toText() const;
    bool fromText(const std::string& v);
    void execute() const;
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
#endif // mrWithOpenCV

} // namespace mr
