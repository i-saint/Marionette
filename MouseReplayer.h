#pragma once


#define mrAPI extern "C" __declspec(dllexport)

using millisec = uint64_t;

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


class Recorder
{
public:
    bool startRecording();
    bool stopRecording();
    bool update();
    bool save(const char* path) const;

    // internal
    void addRecord(OpRecord rec);
    void onInput(const RAWINPUT& raw);

private:
    bool m_recording = false;
    millisec m_time_start = 0;
    HWND m_hwnd = nullptr;
    bool m_lb = false, m_rb = false, m_mb = false;
    int m_x = 0, m_y = 0;

    std::vector<OpRecord> m_records;
};

class Player
{
public:
    bool startReplay(uint32_t loop = 1);
    bool stopReplay();
    bool update();
    bool load(const char* path);

private:
    bool m_playing = false;
    millisec m_time_start = 0;
    uint32_t m_record_index = 0;
    uint32_t m_loop_required = 0, m_loop_current = 0;
    std::vector<OpRecord> m_records;
};


void Print(const char* fmt, ...);
#ifdef mrDebug
    #define DbgPrint(...) Print(__VA_ARGS__)
#else
    #define DbgPrint(...)
#endif
