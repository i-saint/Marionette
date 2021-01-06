#include "pch.h"
#include "MouseReplayer.h"

#pragma comment(lib,"user32.lib")



std::string OpRecord::toText() const
{
    char buf[256]{};
    switch (type)
    {
    case OpType::Wait:
        snprintf(buf, sizeof(buf), "Wait [%d]", time);
        break;

    case OpType::MouseMove:
        snprintf(buf, sizeof(buf), "MouseMove %d %d [%d]", data.mouse.x, data.mouse.y, time);
        break;

    case OpType::MouseDown:
        snprintf(buf, sizeof(buf), "MouseDown %d [%d]", data.mouse.button, time);
        break;

    case OpType::MouseUp:
        snprintf(buf, sizeof(buf), "MouseUp %d [%d]", data.mouse.button, time);
        break;

    case OpType::KeyDown:
        snprintf(buf, sizeof(buf), "KeyDown %d [%d]", data.key.code, time);
        break;

    case OpType::KeyUp:
        snprintf(buf, sizeof(buf), "KeyUp %d [%d]", data.key.code, time);
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
    if (sscanf(src, "Wait [%d]", &time) == 1)
        type = OpType::Wait;
    else if (sscanf(src, "MouseMove %d %d [%d]", &data.mouse.x, &data.mouse.y, &time) == 3)
        type = OpType::MouseMove;
    else if (sscanf(src, "MouseDown %d [%d]", &data.mouse.button, &time) == 2)
        type = OpType::MouseDown;
    else if (sscanf(src, "MouseUp %d [%d]", &data.mouse.button, &time) == 2)
        type = OpType::MouseUp;
    else if (sscanf(src, "KeyDown %d [%d]", &data.key.code, &time) == 2)
        type = OpType::KeyDown;
    else if (sscanf(src, "KeyUp %d [%d]", &data.key.code, &time) == 2)
        type = OpType::KeyUp;
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
            LONG screen_width = ::GetSystemMetrics(SM_CXSCREEN) - 1;
            LONG screen_height = ::GetSystemMetrics(SM_CYSCREEN) - 1;
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


millisec NowMS()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void SleepMS(millisec v)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(v));
}



bool Recorder::startRecording()
{
    if (m_recording)
        return false;

    m_time_start = NowMS();
    m_recording = true;
    return true;
}

bool Recorder::stopRecording()
{
    // nothing to do for now
    return true;
}

bool Recorder::update()
{
    bool button_changed = false;

    auto addMoveRecord = [&]() {
        CURSORINFO ci;
        ci.cbSize = sizeof(ci);
        ::GetCursorInfo(&ci);

        OpRecord rec;
        rec.type = OpType::MouseMove;
        m_x = rec.data.mouse.x = ci.ptScreenPos.x;
        m_y = rec.data.mouse.y = ci.ptScreenPos.y;
        addRecord(rec);
    };

    auto addButtonRecord = [&](bool down, int button) {
        addMoveRecord();

        OpRecord rec;
        rec.type = down ? OpType::MouseDown : OpType::MouseUp;
        rec.data.mouse.button = button;
        addRecord(rec);

        button_changed = true;
    };


    bool lb = ::GetKeyState(VK_LBUTTON) & 0x80;
    bool rb = ::GetKeyState(VK_RBUTTON) & 0x80;
    bool mb = ::GetKeyState(VK_MBUTTON) & 0x80;
    if (m_lb != lb) {
        m_lb = lb;
        addButtonRecord(lb, 1);
    }
    if (m_rb != rb) {
        m_rb = rb;
        addButtonRecord(rb, 2);
    }
    if (m_mb != mb) {
        m_mb = mb;
        addButtonRecord(mb, 3);
    }

    if (!button_changed && (m_lb || m_rb || m_mb)) {
        CURSORINFO ci;
        ci.cbSize = sizeof(ci);
        ::GetCursorInfo(&ci);
        if (m_x != ci.ptScreenPos.x || m_y != ci.ptScreenPos.y)
            addMoveRecord();
    }

    if (::GetKeyState(VK_ESCAPE) & 0x80) {
        OpRecord rec;
        rec.type = OpType::Wait;
        addRecord(rec);

        m_recording = false;
    }

    SleepMS(1);
    return m_recording;
}

void Recorder::addRecord(OpRecord rec)
{
    if (!m_recording)
        return;

    rec.time = NowMS() - m_time_start;
    m_records.push_back(rec);
}

bool Recorder::save(const char* path) const
{
    std::ofstream ofs(path, std::ios::out);
    if (!ofs)
        return false;

    for (auto& rec : m_records)
        ofs << rec.toText() << std::endl;
    return true;
}



bool Player::startReplay(uint32_t loop)
{
    if (m_playing)
        return false;

    m_time_start = NowMS();
    m_loop_required = loop;
    m_loop_current = 0;
    m_record_index = 0;
    m_playing = true;
    return true;
}

bool Player::stopReplay()
{
    if (!m_playing)
        return false;

    m_playing = false;
    return true;
}

bool Player::update()
{
    if (!m_playing || m_records.empty() || m_loop_current >= m_loop_required)
        return false;

    millisec now = NowMS() - m_time_start;
    for (;;) {
        const auto& rec = m_records[m_record_index];
        if (now >= rec.time) {
            rec.execute();
            ++m_record_index;

            if (m_record_index == m_records.size()) {
                ++m_loop_current;
                m_record_index = 0;
                m_time_start = NowMS();
                break;
            }
        }
        else {
            break;
        }
    }

    if (::GetKeyState(VK_ESCAPE) & 0x80) {
        m_playing = false;
        return false;
    }

    SleepMS(1);
    return true;
}

bool Player::load(const char* path)
{
    m_records.clear();

    std::ifstream ifs(path, std::ios::in);
    if (!ifs)
        return false;

    std::string l;
    while (std::getline(ifs, l)) {
        OpRecord rec;
        if (rec.fromText(l))
            m_records.push_back(rec);
    }
    std::stable_sort(m_records.begin(), m_records.end(),
        [](auto& a, auto& b) { return a.time < b.time; });

    return !m_records.empty();
}
