#include "pch.h"
#include "MouseReplayer.h"

// note:
// SetWindowsHookEx() is ideal for recording mouse, but it is it is too restricted after Windows Vista.
// (requires admin privilege, UI access, etc.
//  https://www.wintellect.com/so-you-want-to-set-a-windows-journal-recording-hook-on-vista-it-s-not-nearly-as-easy-as-you-think/ )
// so, use low level input API instead.

static LRESULT CALLBACK MouseRecorderProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_INPUT) {
        auto hRawInput = (HRAWINPUT)lParam;
        UINT dwSize = 0;
        char buf[1024];
        // first GetRawInputData() get dwSize, second one get RAWINPUT data.
        ::GetRawInputData(hRawInput, RID_INPUT, buf, &dwSize, sizeof(RAWINPUTHEADER));
        ::GetRawInputData(hRawInput, RID_INPUT, buf, &dwSize, sizeof(RAWINPUTHEADER));

        auto recorder = (Recorder*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if (recorder)
            recorder->onInput(*(RAWINPUT*)buf);

        return 0;
    }

    return ::DefWindowProc(hWnd, Msg, wParam, lParam);
}

void Recorder::onInput(const RAWINPUT& raw)
{
    if (raw.header.dwType == RIM_TYPEMOUSE) {
        bool button_changed = false;

        auto addMoveRecord = [&]() {
            // RAWMOUSE provide only relative mouse position (in most cases)
            // so GetCursorInfo() to get absolute position
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


        auto& rawMouse = raw.data.mouse;
        auto buttons = rawMouse.usButtonFlags;

        // handle buttons
        if (buttons & RI_MOUSE_LEFT_BUTTON_DOWN) {
            m_lb = true;
            addButtonRecord(m_lb, 1);
        }
        else if (buttons & RI_MOUSE_LEFT_BUTTON_UP) {
            m_lb = false;
            addButtonRecord(m_lb, 1);
        }
        else if (buttons & RI_MOUSE_RIGHT_BUTTON_DOWN) {
            m_rb = true;
            addButtonRecord(m_rb, 1);
        }
        else if (buttons & RI_MOUSE_RIGHT_BUTTON_UP) {
            m_rb = false;
            addButtonRecord(m_rb, 1);
        }
        else if (buttons & RI_MOUSE_MIDDLE_BUTTON_DOWN) {
            m_mb = true;
            addButtonRecord(m_mb, 1);
        }
        else if (buttons & RI_MOUSE_MIDDLE_BUTTON_UP) {
            m_mb = false;
            addButtonRecord(m_mb, 1);
        }
        else if (buttons == 0) {
            // handle dragging
            if (m_lb || m_rb || m_mb) {
                CURSORINFO ci;
                ci.cbSize = sizeof(ci);
                ::GetCursorInfo(&ci);
                if (m_x != ci.ptScreenPos.x || m_y != ci.ptScreenPos.y)
                    addMoveRecord();
            }
        }
    }
    else if (raw.header.dwType == RIM_TYPEKEYBOARD) {
        // stop recording if escape is pressed
        if (raw.data.keyboard.VKey == VK_ESCAPE) {
            OpRecord rec;
            rec.type = OpType::Wait;
            addRecord(rec);
            stopRecording();
        }
    }
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
