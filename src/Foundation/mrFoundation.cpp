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
}

const char* Scan(const char* s, const std::regex& exp, const std::function<void(std::cmatch& m)>& body)
{
    std::cmatch match;
    for (;;) {
        std::regex_search(s, match, exp);
        if (!match.empty()) {
            body(match);
            s += match.position() + match.length();
        }
        else {
            break;
        }
    }
    return s;
}

const char* Scan(const std::string& str, const std::regex& exp, const std::function<void(std::cmatch& m)>& body)
{
    return Scan(str.c_str(), exp, body);
}

const char* Scan(const std::string& str, const std::regex& exp, const std::function<void(std::string sub)>& body)
{
    return Scan(str, exp, [&body](std::cmatch& m) { body(m.str(1)); });
}

const char* ScanKVP(const char* s, const std::function<void(std::string k, std::string v)>& body)
{
    auto ex_key = std::regex(R"((\w+)\s*:\s*)");
    auto ex_value = std::regex(R"(^([^ ]+))");
    auto ex_parenthesis = std::regex(R"(^\{([^}]+)\})");
    std::cmatch match;
    for (;;) {
        std::regex_search(s, match, ex_key);
        if (!match.empty()) {
            std::string key, value;
            key = match.str(1);
            s += match.position() + match.length();

            if (std::regex_search(s, match, ex_parenthesis) || std::regex_search(s, match, ex_value)) {
                value = match.str(1);
                s += match.position() + match.length();

                body(key, value);
            }
            else {
                break;
            }
        }
        else {
            break;
        }
    }
    return s;
}

const char* ScanKVP(const std::string& str, const std::function<void(std::string k, std::string v)>& body)
{
    return ScanKVP(str.c_str(), body);
}


std::string Replace(const std::string& str, const std::string& before, const std::string& after)
{
    auto pos = str.find(before);
    if (pos != std::string::npos) {
        std::string ret;
        ret += std::string_view(str.begin(), str.begin() + pos);
        ret += after;
        ret += std::string_view(str.begin() + (pos + before.size()), str.end());
        return ret;
    }
    else {
        return str;
    }
}

// T: int, float, float2, std::string
template<> int ToValue(const std::string& str) { return std::stoi(str); }
template<> float ToValue(const std::string& str) { return std::stof(str); }
template<> float2 ToValue(const std::string& str)
{
    float2 r{};
    if (sscanf(str.c_str(), "%f,%f", &r.x, & r.y) == 2)
        return r;
    return float2{};
}
template<> std::string ToValue(const std::string& str)
{
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"')
        return std::string(str.begin() + 1, str.end() - 1);
    return "";
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
    auto& handlers = GetInitializeHandlers();
    for (auto& h : handlers)
        h();
}

mrAPI void Finalize()
{
    auto& handlers = GetFinalizeHandlers();
    for (auto& h : handlers | std::views::reverse)
        h();
}

} // namespace mr
