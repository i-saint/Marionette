#include "pch.h"
#include "MouseReplayer.h"

#pragma comment(lib,"user32.lib")


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


// note:
// SetWindowsHookEx() is ideal for recording mouse, but it is it is too restricted after Windows Vista.
// (requires admin privilege, UI access, etc.
//  https://www.wintellect.com/so-you-want-to-set-a-windows-journal-recording-hook-on-vista-it-s-not-nearly-as-easy-as-you-think/ )
// so, use low level input API instead.

static LRESULT CALLBACK MouseRecorderProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    static bool s_lb = false, s_rb = false, s_mb = false;
    static int s_x = 0, s_y = 0;

    if (Msg == WM_INPUT) {
        auto recorder = (Recorder*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);

        auto hRawInput = (HRAWINPUT)lParam;
        UINT dwSize = 0;
        char buf[1024];
        auto raw = (RAWINPUT*)buf;
        // first GetRawInputData() get dwSize, second one get RAWINPUT data.
        ::GetRawInputData(hRawInput, RID_INPUT, buf, &dwSize, sizeof(RAWINPUTHEADER));
        if (dwSize >= sizeof(buf)) {
            DbgPrint("*** GetRawInputData() buffer size exceeded ***\n");
            return 0;
        }
        ::GetRawInputData(hRawInput, RID_INPUT, buf, &dwSize, sizeof(RAWINPUTHEADER));

        if (raw->header.dwType == RIM_TYPEMOUSE) {
            bool button_changed = false;

            auto addMoveRecord = [&]() {
                // RAWMOUSE provide only relative mouse position (in most cases)
                // so GetCursorInfo() to get absolute position
                CURSORINFO ci;
                ci.cbSize = sizeof(ci);
                ::GetCursorInfo(&ci);

                OpRecord rec;
                rec.type = OpType::MouseMove;
                s_x = rec.data.mouse.x = ci.ptScreenPos.x;
                s_y = rec.data.mouse.y = ci.ptScreenPos.y;
                recorder->addRecord(rec);
            };

            auto addButtonRecord = [&](bool down, int button) {
                addMoveRecord();

                OpRecord rec;
                rec.type = down ? OpType::MouseDown : OpType::MouseUp;
                rec.data.mouse.button = button;
                recorder->addRecord(rec);

                button_changed = true;
            };


            auto& rawMouse = raw->data.mouse;
            auto buttons = rawMouse.usButtonFlags;

            // handle buttons
            if (buttons & RI_MOUSE_LEFT_BUTTON_DOWN) {
                s_lb = true;
                addButtonRecord(s_lb, 1);
            }
            else if (buttons & RI_MOUSE_LEFT_BUTTON_UP) {
                s_lb = false;
                addButtonRecord(s_lb, 1);
            }
            else if (buttons & RI_MOUSE_RIGHT_BUTTON_DOWN) {
                s_rb = true;
                addButtonRecord(s_rb, 1);
            }
            else if (buttons & RI_MOUSE_RIGHT_BUTTON_UP) {
                s_rb = false;
                addButtonRecord(s_rb, 1);
            }
            else if (buttons & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
                s_mb = true;
                addButtonRecord(s_mb, 1);
            }
            else if (buttons & RI_MOUSE_MIDDLE_BUTTON_UP) {
                s_mb = false;
                addButtonRecord(s_mb, 1);
            }
            else if (buttons == 0) {
                // handle dragging
                if (s_lb || s_rb || s_mb) {
                    CURSORINFO ci;
                    ci.cbSize = sizeof(ci);
                    ::GetCursorInfo(&ci);
                    if (s_x != ci.ptScreenPos.x || s_y != ci.ptScreenPos.y)
                        addMoveRecord();
                }
            }
        }
        else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
            // stop recording if escape is pressed
            if (raw->data.keyboard.VKey == VK_ESCAPE) {
                OpRecord rec;
                rec.type = OpType::Wait;
                recorder->addRecord(rec);
                recorder->stopRecording();
            }
        }
        return 0;
    }

    return ::DefWindowProc(hWnd, Msg, wParam, lParam);
}

bool Recorder::startRecording()
{
    if (m_recording)
        return false;

    WNDCLASS wx = {};
    wx.lpfnWndProc = &MouseRecorderProc;
    wx.hInstance = ::GetModuleHandle(nullptr);
    wx.lpszClassName = TEXT("MouseRecorderClass");
    if (!::RegisterClass(&wx)) {
        DbgPrint("*** RegisterClassEx() failed ***\n");
        return false;
    }

    m_hwnd = ::CreateWindow(wx.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wx.hInstance, nullptr);
    if (!m_hwnd) {
        DbgPrint("*** CreateWindowEx() failed ***\n");
        return false;
    }
    ::SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    RAWINPUTDEVICE rid[2]{};
    // mouse
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x02;
    rid[0].dwFlags = RIDEV_NOLEGACY | RIDEV_INPUTSINK;
    rid[0].hwndTarget = m_hwnd;
    // keyboard
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage = 0x06;
    rid[1].dwFlags = RIDEV_NOLEGACY | RIDEV_INPUTSINK;
    rid[1].hwndTarget = m_hwnd;
    if (!::RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
        DbgPrint("*** RegisterRawInputDevices() failed ***\n");
        stopRecording();
        return false;
    }

    m_time_start = NowMS();
    m_recording = true;
    return true;
}

bool Recorder::stopRecording()
{
    if (m_hwnd) {
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_recording = false;

    return true;
}

bool Recorder::update()
{
    MSG msg{};
    if (::GetMessage(&msg, m_hwnd, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
    return m_recording;
}

void Recorder::addRecord(OpRecord rec)
{
    if (!m_recording)
        return;

    rec.time = NowMS() - m_time_start;
    m_records.push_back(rec);
    DbgPrint("record added: %s\n", rec.toText().c_str());
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
    // execute records
    for (;;) {
        const auto& rec = m_records[m_record_index];
        if (now >= rec.time) {
            rec.execute();
            ++m_record_index;
            DbgPrint("record executed: %s\n", rec.toText().c_str());

            if (m_record_index == m_records.size()) {
                // go next loop or stop
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

    // stop if escape key is pressed
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
