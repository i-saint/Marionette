#include "pch.h"
#include "Internal.h"

namespace mr {

// note:
// SetWindowsHookEx() is ideal for recording mouse, but it is it is too restricted after Windows Vista.
// (requires admin privilege, UI access, etc.
//  https://www.wintellect.com/so-you-want-to-set-a-windows-journal-recording-hook-on-vista-it-s-not-nearly-as-easy-as-you-think/ )
// so, use low level input API instead.

class InputReceiver : public IInputReceiver
{
public:
    static InputReceiver& instance();

    bool valid() const override;
    void update() override;

    int  addHandler(OpRecordHandler v) override;
    void removeHandler(int i) override;

    int  addRecorder(OpRecordHandler v) override;
    void removeRecorder(int i) override;

    // internal
    void onInput(RAWINPUT& raw);
    static LRESULT CALLBACK receiverProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

private:
    InputReceiver();
    ~InputReceiver();
    InputReceiver(const InputReceiver&) = delete;

    HWND m_hwnd = nullptr;
    bool m_lb = false, m_rb = false, m_mb = false;
    int m_x = 0, m_y = 0;

    int m_handler_seed = 0;
    int m_recorder_seed = 0;
    std::map<int, OpRecordHandler> m_handlers;
    std::map<int, OpRecordHandler> m_recorders;
};

InputReceiver::InputReceiver()
{
    // create invisible window to receive input messages
    WNDCLASS wc{};
    wc.lpfnWndProc = &receiverProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.lpszClassName = TEXT("mrInputReceiverClass");
    if (!::RegisterClass(&wc)) {
        mrDbgPrint("*** RegisterClassEx() failed ***\n");
        return;
    }

    m_hwnd = ::CreateWindow(wc.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!m_hwnd) {
        mrDbgPrint("*** CreateWindowEx() failed ***\n");
        return;
    }
    ::SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    // register rawinput devices
    RAWINPUTDEVICE rid[2]{};
    // mouse
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x02;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = m_hwnd;
    // keyboard
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage = 0x06;
    rid[1].dwFlags = RIDEV_INPUTSINK;
    rid[1].hwndTarget = m_hwnd;
    if (!::RegisterRawInputDevices(rid, std::size(rid), sizeof(RAWINPUTDEVICE))) {
        mrDbgPrint("*** RegisterRawInputDevices() failed ***\n");
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

InputReceiver::~InputReceiver()
{
    if (m_hwnd) {
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void InputReceiver::onInput(RAWINPUT& raw)
{
    auto dispatchRecord = [this](OpRecord& rec) {
        for (auto& h : m_handlers) {
            if (!h.second(rec))
                return;
        }
        for (auto& h : m_recorders) {
            h.second(rec);
        }
    };

    if (raw.header.dwType == RIM_TYPEMOUSE) {
        // handle mouse

        auto addButtonRecord = [&](bool down, int button) {
            OpRecord rec;
            rec.type = down ? OpType::MouseDown : OpType::MouseUp;
            rec.data.mouse.button = button;
            dispatchRecord(rec);
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
            // handle move
            // 
            // RAWMOUSE provide only relative mouse position (in most cases)
            // so use GetCursorInfo() to get absolute position...

            CURSORINFO ci;
            ci.cbSize = sizeof(ci);
            ::GetCursorInfo(&ci);
            if (m_x != ci.ptScreenPos.x || m_y != ci.ptScreenPos.y) {
                OpRecord rec;
                rec.type = OpType::MouseMoveAbs;
                m_x = rec.data.mouse.x = ci.ptScreenPos.x;
                m_y = rec.data.mouse.y = ci.ptScreenPos.y;
                dispatchRecord(rec);
            }
        }
    }
    else if (raw.header.dwType == RIM_TYPEKEYBOARD) {
        // handle keyboard
        OpRecord rec;
        if (raw.data.keyboard.Flags & RI_KEY_BREAK)
            rec.type = OpType::KeyUp;
        else
            rec.type = OpType::KeyDown;

        rec.data.key.code = raw.data.keyboard.VKey;
        dispatchRecord(rec);
    }
}

LRESULT CALLBACK InputReceiver::receiverProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_INPUT) {
        auto hRawInput = (HRAWINPUT)lParam;
        UINT dwSize = 0;
        char buf[256];
        // first one get dwSize, second one get RAWINPUT data.
        // in this case size of raw input data can be assumed constant because device is mouse or keyboard.
        ::GetRawInputData(hRawInput, RID_INPUT, buf, &dwSize, sizeof(RAWINPUTHEADER));
        ::GetRawInputData(hRawInput, RID_INPUT, buf, &dwSize, sizeof(RAWINPUTHEADER));

        InputReceiver::instance().onInput(*(RAWINPUT*)buf);
    }

    return ::DefWindowProc(hWnd, Msg, wParam, lParam);
}

InputReceiver& InputReceiver::instance()
{
    static InputReceiver s_instance;
    return s_instance;
}

bool InputReceiver::valid() const
{
    return m_hwnd != nullptr;
}

void InputReceiver::update()
{
    if (m_hwnd) {
        MSG msg{};
        while (::PeekMessage(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }
}

int InputReceiver::addHandler(OpRecordHandler v)
{
    int ret = ++m_handler_seed;
    m_handlers[ret] = v;
    return ret;
}

void InputReceiver::removeHandler(int i)
{
    m_handlers.erase(i);
}

int InputReceiver::addRecorder(OpRecordHandler v)
{
    int ret = ++m_recorder_seed;
    m_recorders[ret] = v;
    return ret;
}

void InputReceiver::removeRecorder(int i)
{
    m_recorders.erase(i);
}

mrAPI IInputReceiver* GetReceiver()
{
    return &InputReceiver::instance();
}

} // namespace mr
