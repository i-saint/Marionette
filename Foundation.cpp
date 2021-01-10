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

void SleepMS(millisec v)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(v));
}



std::string OpRecord::toText() const
{
    char buf[256]{};
    switch (type)
    {
    case OpType::Wait:
        snprintf(buf, sizeof(buf), "%lld: Wait", time);
        break;

    case OpType::PushState:
        snprintf(buf, sizeof(buf), "%lld: PushState", time);
        break;

    case OpType::PopState:
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
        type = OpType::PushState;
    else if (std::strstr(src, "PopState") && sscanf(src, "%lld: ", &time) == 1)
        type = OpType::PopState;
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


} // namespace mr
