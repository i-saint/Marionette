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
