#pragma once

#define mrAPI extern "C" __declspec(dllexport)

namespace mr {

using millisec = uint64_t;

#ifdef mrDebug
    #define DbgPrint(...) Print(__VA_ARGS__)
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
    millisec time = 0;
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

class IRecorder
{
public:
    virtual ~IRecorder() {}
    virtual void release() = 0;
    virtual bool startRecording() = 0;
    virtual bool stopRecording() = 0;
    virtual bool update() = 0;
    virtual bool save(const char* path) const = 0;
};

class IPlayer
{
public:
    virtual ~IPlayer() {}
    virtual void release() = 0;
    virtual bool startReplay(uint32_t loop = 1) = 0;
    virtual bool stopReplay() = 0;
    virtual bool update() = 0;
    virtual bool load(const char* path) = 0;
};

mrAPI IRecorder* CreateRecorder();
mrAPI IPlayer* CreatePlayer();


// utils

template<class T>
struct releaser
{
    void operator()(T* p) { p->release(); }
};

inline std::shared_ptr<IRecorder> CreateRecorderShared()
{
    return std::shared_ptr<IRecorder>(CreateRecorder(), releaser<IRecorder>());
}

inline std::shared_ptr<IPlayer> CreatePlayerShared()
{
    return std::shared_ptr<IPlayer>(CreatePlayer(), releaser<IPlayer>());
}

} // namespace mr
