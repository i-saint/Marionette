#include "pch.h"
#include "MouseReplayer.h"

#pragma comment(lib,"user32.lib")

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

void SleepMS(millisec v)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(v));
}



std::string OpRecord::toText() const
{
    char buf[256]{};
    switch (type)
    {
    case OpType::MouseMove:
        snprintf(buf, sizeof(buf), "%lld: MouseMove %d %d", time, data.mouse.x, data.mouse.y);
        break;

    case OpType::MouseDown:
        snprintf(buf, sizeof(buf), "%lld: MouseDown %d", time, data.mouse.button);
        break;

    case OpType::MouseUp:
        snprintf(buf, sizeof(buf), "%lld: MouseUp %d", time, data.mouse.button);
        break;

    case OpType::KeyDown:
        snprintf(buf, sizeof(buf), "%lld: KeyDown %d", time, data.key.code);
        break;

    case OpType::KeyUp:
        snprintf(buf, sizeof(buf), "%lld: KeyUp %d", time, data.key.code);
        break;

    case OpType::Wait:
        snprintf(buf, sizeof(buf), "%lld: Wait", time);
        break;

    default:
        break;
    }
    return buf;
}

bool OpRecord::fromText(const std::string& v)
{
    type = OpType::Unknown;

    const char* src = v.c_str();
    if (sscanf(src, "%lld: MouseMove %d %d", &time, &data.mouse.x, &data.mouse.y) == 3)
        type = OpType::MouseMove;
    else if (sscanf(src, "%lld: MouseDown %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseDown;
    else if (sscanf(src, "%lld: MouseUp %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseUp;
    else if (sscanf(src, "%lld: KeyDown %d", &time, &data.key.code) == 2)
        type = OpType::KeyDown;
    else if (sscanf(src, "%lld: KeyUp %d", &time, &data.key.code) == 2)
        type = OpType::KeyUp;
    else if (sscanf(src, "%lld: Wait", &time) == 1)
        type = OpType::Wait;
    return type != OpType::Unknown;
}

void OpRecord::execute() const
{
    switch (type)
    {
    case OpType::MouseMove:
    case OpType::MouseDown:
    case OpType::MouseUp:
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        if (type == OpType::MouseMove) {
            // handle mouse position
            // 
            // http://msdn.microsoft.com/en-us/library/ms646260(VS.85).aspx
            // If MOUSEEVENTF_ABSOLUTE value is specified, dx and dy contain normalized absolute coordinates between 0 and 65,535.
            // The event procedure maps these coordinates onto the display surface.
            // Coordinate (0,0) maps onto the upper-left corner of the display surface, (65535,65535) maps onto the lower-right corner.

            LONG screen_width = ::GetSystemMetrics(SM_CXSCREEN) - 1; // SM_CXVIRTUALSCREEN
            LONG screen_height = ::GetSystemMetrics(SM_CYSCREEN) - 1; // SM_CYVIRTUALSCREEN
            input.mi.dx = (LONG)(data.mouse.x * (65535.0f / screen_width));
            input.mi.dy = (LONG)(data.mouse.y * (65535.0f / screen_height));
            input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        }
        else if (type == OpType::MouseDown) {
            // handle buttons
            switch (data.mouse.button) {
            case 1: input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN; break;
            case 2: input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN; break;
            case 3: input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN; break;
            default: break;
            }
        }
        else if (type == OpType::MouseUp) {
            // handle buttons
            switch (data.mouse.button) {
            case 1: input.mi.dwFlags |= MOUSEEVENTF_LEFTUP; break;
            case 2: input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP; break;
            case 3: input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP; break;
            default: break;
            }
        }
        ::SendInput(1, &input, sizeof(INPUT));
        break;
    }

    case OpType::KeyDown:
    case OpType::KeyUp:
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = (WORD)data.key.code;

        if (type == OpType::KeyUp)
            input.ki.dwFlags |= KEYEVENTF_KEYUP;

        ::SendInput(1, &input, sizeof(INPUT));
        break;
    }

    default:
        break;
    }
}

} // namespace mr
