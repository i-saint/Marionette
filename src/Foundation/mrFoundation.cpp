#include "pch.h"
#include "mrInternal.h"

#pragma comment(lib, "shcore.lib")

namespace mr {

std::string FormatImpl(const char* format, va_list args)
{
    const int MaxBuf = 4096;
    char buf[MaxBuf];
    vsprintf(buf, format, args);
    return buf;
}

std::string Format(const char* format, ...)
{
    std::string ret;
    va_list args;
    va_start(args, format);
    ret = FormatImpl(format, args);
    va_end(args);
    fflush(stdout);
    return ret;
}

void Print(const char* fmt, ...)
{
    char buf[1024 * 2];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, std::size(buf), fmt, args);
    va_end(args);
    ::OutputDebugStringA(buf);
}

void Print(const wchar_t* fmt, ...)
{
    wchar_t buf[1024 * 2];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf(buf, std::size(buf), fmt, args);
    va_end(args);
    ::OutputDebugStringW(buf);
}

millisec NowMS()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

nanosec NowNS()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

void SleepMS(millisec v)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(v));
}

std::string GetCurrentModuleDirectory()
{
    HMODULE mod{};
    char buf[MAX_PATH]{};
    ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)&GetCurrentModuleDirectory, &mod);
    ::GetModuleFileNameA(mod, buf, std::size(buf));
    return std::string(buf, std::strrchr(buf, '\\'));
}

void Split(const std::string& str, const std::string& separator, const std::function<void(std::string sub)>& body)
{
    size_t offset = 0;
    for (;;) {
        size_t pos = str.find(separator, offset);
        body(str.substr(offset, pos - offset));
        if (pos == std::string::npos)
            break;
        else
            offset = pos + separator.size();
    }
};

void Scan(const std::string& str, const std::regex& exp, const std::function<void(std::string sub)>& body)
{
    std::cmatch match;
    const char* s = str.c_str();
    for (;;) {
        std::regex_search(s, match, exp);
        if (!match.empty()) {
            body(match.str(1));
            s += match.position() + match.length();
        }
        else {
            break;
        }
    }

}

Timer::Timer()
{
    reset();
}

void Timer::reset()
{
    m_begin = NowNS();
}

float Timer::elapsed() const
{
    return (NowNS() - m_begin) / 1000000000.0;
}


ProfileTimer::ProfileTimer(const char* mes, ...)
{
    va_list args;
    va_start(args, mes);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), mes, args);
    va_end(args);

    m_message = buf;
}

ProfileTimer::~ProfileTimer()
{
    float t = elapsed() * 1000.0f;
    Print("%s - %.2fms\n", m_message.c_str(), t);
}



std::string OpRecord::toText() const
{
    char buf[256]{};
    switch (type)
    {
    case OpType::Wait:
        snprintf(buf, sizeof(buf), "%lld: Wait", time);
        break;

    case OpType::KeyDown:
        snprintf(buf, sizeof(buf), "%lld: KeyDown %d", time, data.key.code);
        break;

    case OpType::KeyUp:
        snprintf(buf, sizeof(buf), "%lld: KeyUp %d", time, data.key.code);
        break;

    case OpType::MouseDown:
        snprintf(buf, sizeof(buf), "%lld: MouseDown %d", time, data.mouse.button);
        break;

    case OpType::MouseUp:
        snprintf(buf, sizeof(buf), "%lld: MouseUp %d", time, data.mouse.button);
        break;

    case OpType::MouseMoveAbs:
        snprintf(buf, sizeof(buf), "%lld: MouseMoveAbs %d %d", time, data.mouse.pos.x, data.mouse.pos.y);
        break;

    case OpType::MouseMoveRel:
        snprintf(buf, sizeof(buf), "%lld: MouseMoveRel %d %d", time, data.mouse.pos.x, data.mouse.pos.y);
        break;

    case OpType::SaveMousePos:
        snprintf(buf, sizeof(buf), "%lld: SaveMousePos %d", time, exdata.save_slot);
        break;

    case OpType::LoadMousePos:
        snprintf(buf, sizeof(buf), "%lld: LoadMousePos %d", time, exdata.save_slot);
        break;


    case OpType::MatchParams:
    {
        std::string ret;
        auto& p = exdata.match_params;
        ret += "MatchParams";
        ret += Format(" Scale=%.2f", p.scale);
        ret += Format(" CareDisplayScale=%d", (int)p.care_display_scale);
        ret += Format(" ContourBlockSize=%d", p.contour_block_size);
        ret += Format(" ExpandBlockSize=%d", p.expand_block_size);
        ret += Format(" BinarizeThreshold=%d", p.binarize_threshold);
        break;
    }
    case OpType::MouseMoveMatch:
    {
        std::string ret;
        snprintf(buf, sizeof(buf), "%lld: MouseMoveMatch", time);
        ret += buf;
        for (auto& id : exdata.templates) {
            snprintf(buf, sizeof(buf), " \"%s\"", id.path.c_str());
            ret += buf;
        }
        return ret;
    }
    default:
        break;
    }
    return buf;
}

bool OpRecord::fromText(const std::string& v)
{
    type = OpType::Unknown;

    char buf[MAX_PATH]{};
    const char* src = v.c_str();

    auto skip = [&src]() {
        while (*src != '\0') {
            if (src[0] == ' ' && src[1] != ' ') {
                ++src;
                break;
            }
            ++src;
        }
    };

    if (std::strstr(src, "Wait") && sscanf(src, "%lld: ", &time) == 1)
        type = OpType::Wait;
    else if (sscanf(src, "%lld: KeyDown %d", &time, &data.key.code) == 2)
        type = OpType::KeyDown;
    else if (sscanf(src, "%lld: KeyUp %d", &time, &data.key.code) == 2)
        type = OpType::KeyUp;
    else if (sscanf(src, "%lld: MouseDown %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseDown;
    else if (sscanf(src, "%lld: MouseUp %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseUp;
    else if (sscanf(src, "%lld: MouseMoveAbs %d %d", &time, &data.mouse.pos.x, &data.mouse.pos.y) == 3)
        type = OpType::MouseMoveAbs;
    else if (sscanf(src, "%lld: MouseMoveRel %d %d", &time, &data.mouse.pos.x, &data.mouse.pos.y) == 3)
        type = OpType::MouseMoveRel;
    else if (sscanf(src, "%lld: SaveMousePos %d", &time, &exdata.save_slot) == 2)
        type = OpType::SaveMousePos;
    else if (sscanf(src, "%lld: LoadMousePos %d", &time, &exdata.save_slot) == 2)
        type = OpType::LoadMousePos;
    else if (std::strstr(src, "MatchParams ")) {
        skip();

        auto& p = exdata.match_params;
        float fv;
        int iv;
        while (*src != '\0') {
            if (sscanf(src, "Scale=%f", &fv) == 1) p.scale = fv;
            else if (sscanf(src, "CareDisplayScale=%d", &iv) == 1) p.care_display_scale = iv != 0;
            else if (sscanf(src, "ContourBlockSize=%d", &iv) == 1) p.contour_block_size = iv;
            else if (sscanf(src, "ExpandBlockSize=%d", &iv) == 1) p.expand_block_size = iv;
            else if (sscanf(src, "BinarizeThreshold=%d", &iv) == 1) p.binarize_threshold = iv;
            skip();
        }
    }
    else if (std::strstr(src, "MouseMoveMatch") && sscanf(src, "%lld: ", &time) == 1) {
        type = OpType::MouseMoveMatch;
        Scan(src, std::regex("\"([^\"]+)\""), [this](std::string path) {
            exdata.templates.push_back({ 0, path });
            });
    }
    return type != OpType::Unknown;
}

std::map<Key, std::string> LoadKeymap(const char* path, const std::function<void(Key key, std::string path)>& body)
{
    std::map<Key, std::string> ret;

    std::ifstream ifs(path, std::ios::in);
    if (!ifs)
        return ret;

    static std::map<std::string, int> keymap
    {
        {"back", VK_BACK},
        {"tab", VK_TAB},
        {"clear", VK_CLEAR},
        {"enter", VK_RETURN},
        {"pause", VK_PAUSE},
        {"escape", VK_ESCAPE},
        {"space", VK_SPACE},
        {"f1", VK_F1},
        {"f2", VK_F2},
        {"f3", VK_F3},
        {"f4", VK_F4},
        {"f5", VK_F5},
        {"f6", VK_F6},
        {"f7", VK_F7},
        {"f8", VK_F8},
        {"f9", VK_F9},
        {"f10", VK_F10},
        {"f11", VK_F11},
        {"f12", VK_F12},
    };
    static std::regex line("([^:]+):\\s*(.+)");

    std::string l;
    while (std::getline(ifs, l)) {
        std::smatch mline;
        std::regex_match(l, mline, line);
        if (mline.size()) {
            std::string keys = mline[1];
            for (char& c : keys)
                c = std::tolower(c);

            std::smatch mkeys;
            auto match_keys = [&keys, &mkeys](auto& r) {
                std::regex_match(keys, mkeys, r);
                return !mkeys.empty();
            };

            Key key{};
            Split(keys, "+", [&](std::string k) {
                if (k == "ctrl")
                    key.ctrl = 1;
                else if (k == "alt")
                    key.alt = 1;
                else if (k == "shift")
                    key.shift = 1;
                else {
                    if (k.size() == 1) {
                        key.code = std::toupper(k[0]);
                    }
                    else {
                        auto i = keymap.find(k);
                        if (i != keymap.end())
                            key.code = i->second;
                    }
                }
                });

            if (key.code)
                body(key, mline[2].str());
        }
    }
    return ret;
}



static std::vector<std::function<void()>>& GetInitializeHandlers()
{
    static std::vector<std::function<void()>> s_obj;
    return s_obj;
}

static std::vector<std::function<void()>>& GetFinalizeHandlers()
{
    static std::vector<std::function<void()>> s_obj;
    return s_obj;
}

void AddInitializeHandler(const std::function<void()>& v)
{
    GetInitializeHandlers().push_back(v);
}

void AddFinalizeHandler(const std::function<void()>& v)
{
    GetFinalizeHandlers().push_back(v);
}

mrAPI void Initialize()
{
    for (auto& h : GetInitializeHandlers())
        h();
}

mrAPI void Finalize()
{
    for (auto& h : GetFinalizeHandlers() | std::views::reverse)
        h();
}

} // namespace mr
