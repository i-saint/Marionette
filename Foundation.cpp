#include "pch.h"
#include "MouseReplayer.h"


namespace mr {

void Print(const char* fmt, ...)
{
    char buf[1024*2];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

#ifdef _WIN32
    ::OutputDebugStringA(buf);
#else
    printf("%s", buf);
#endif
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

    case OpType::SaveMousePos:
        snprintf(buf, sizeof(buf), "%lld: PushState", time);
        break;

    case OpType::LoadMousePos:
        snprintf(buf, sizeof(buf), "%lld: PopState", time);
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
        snprintf(buf, sizeof(buf), "%lld: MouseMoveAbs %d %d", time, data.mouse.x, data.mouse.y);
        break;

    case OpType::MouseMoveRel:
        snprintf(buf, sizeof(buf), "%lld: MouseMoveRel %d %d", time, data.mouse.x, data.mouse.y);
        break;

    case OpType::MouseMoveMatch:
        snprintf(buf, sizeof(buf), "%lld: MouseMoveMatch %s", time, data.mouse.image_path.c_str());
        break;

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
    if (std::strstr(src, "Wait") && sscanf(src, "%lld: ", &time) == 1)
        type = OpType::Wait;
    else if (std::strstr(src, "PushState") && sscanf(src, "%lld: ", &time) == 1)
        type = OpType::SaveMousePos;
    else if (std::strstr(src, "PopState") && sscanf(src, "%lld: ", &time) == 1)
        type = OpType::LoadMousePos;
    else if (sscanf(src, "%lld: KeyDown %d", &time, &data.key.code) == 2)
        type = OpType::KeyDown;
    else if (sscanf(src, "%lld: KeyUp %d", &time, &data.key.code) == 2)
        type = OpType::KeyUp;
    else if (sscanf(src, "%lld: MouseDown %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseDown;
    else if (sscanf(src, "%lld: MouseUp %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseUp;
    else if (sscanf(src, "%lld: MouseMoveAbs %d %d", &time, &data.mouse.x, &data.mouse.y) == 3)
        type = OpType::MouseMoveAbs;
    else if (sscanf(src, "%lld: MouseMoveRel %d %d", &time, &data.mouse.x, &data.mouse.y) == 3)
        type = OpType::MouseMoveRel;
    else if (sscanf(src, "%lld: MouseMoveMatch \"%[^\"]\"", &time, buf) == 2) {
        data.mouse.image_path = buf;
        type = OpType::MouseMoveMatch;
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

            auto split = [](std::string str, std::string separator, const auto& body) {
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

            Key key{};
            split(keys, "+", [&](std::string k) {
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

} // namespace mr
