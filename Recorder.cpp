#include "pch.h"
#include "MouseReplayer.h"

namespace mr {


// note:
// SetWindowsHookEx() is ideal for recording mouse, but it is it is too restricted after Windows Vista.
// (requires admin privilege, UI access, etc.
//  https://www.wintellect.com/so-you-want-to-set-a-windows-journal-recording-hook-on-vista-it-s-not-nearly-as-easy-as-you-think/ )
// so, use low level input API instead.

class InputReceiver
{
public:
    static InputReceiver& instance();

    bool valid() const;
    void update();

    int addHandler(OpRecordHandler v);
    void removeHandler(int i);

    int addRecorder(OpRecordHandler v);
    void removeRecorder(int i);

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
    WNDCLASS wx = {};
    wx.lpfnWndProc = &receiverProc;
    wx.hInstance = ::GetModuleHandle(nullptr);
    wx.lpszClassName = TEXT("mrInputReceiverClass");
    if (!::RegisterClass(&wx)) {
        DbgPrint("*** RegisterClassEx() failed ***\n");
        return;
    }

    m_hwnd = ::CreateWindow(wx.lpszClassName, nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, wx.hInstance, nullptr);
    if (!m_hwnd) {
        DbgPrint("*** CreateWindowEx() failed ***\n");
        return;
    }
    ::SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

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
    if (!::RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
        DbgPrint("*** RegisterRawInputDevices() failed ***\n");
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
                rec.type = OpType::MouseMove;
                m_x = rec.data.mouse.x = ci.ptScreenPos.x;
                m_y = rec.data.mouse.y = ci.ptScreenPos.y;
                dispatchRecord(rec);
            }
        }
    }
    else if (raw.header.dwType == RIM_TYPEKEYBOARD) {
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
        // first GetRawInputData() get dwSize, second one get RAWINPUT data.
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



class Recorder : public IRecorder
{
public:
    ~Recorder() override;
    void release() override;
    bool start() override;
    bool stop() override;
    bool isRecording() const override;
    bool update() override;
    bool save(const char* path) const override;

    void addRecord(const OpRecord& rec) override;

    // internal

private:
    bool m_recording = false;
    millisec m_time_start = 0;
    int m_handle = 0;

    std::vector<OpRecord> m_records;
};

Recorder::~Recorder()
{
    stop();
}

void Recorder::release()
{
    delete this;
}

bool Recorder::start()
{
    if (m_recording)
        return false;

    auto& receiver = InputReceiver::instance();
    if (!receiver.valid())
        return false;

    m_handle = receiver.addRecorder(
        [this](OpRecord& rec) {
            addRecord(rec);
            return true;
        });

    m_time_start = NowMS();
    m_recording = true;
    return true;
}

bool Recorder::stop()
{
    if (m_recording) {
        m_recording = false;
        mr::OpRecord rec;
        rec.type = mr::OpType::Wait;
        addRecord(rec);
    }
    if (m_handle) {
        InputReceiver::instance().removeRecorder(m_handle);
        m_handle = 0;
    }
    return true;
}

bool Recorder::isRecording() const
{
    return this && m_recording;
}

bool Recorder::update()
{
    InputReceiver::instance().update();
    return m_handle != 0;
}

void Recorder::addRecord(const OpRecord& rec_)
{
    OpRecord rec = rec_;
    if (rec.time == -1) {
        rec.time = NowMS() - m_time_start;
    }
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

mrAPI int AddInputHandler(const OpRecordHandler& handler)
{
    return InputReceiver::instance().addHandler(handler);
}

mrAPI void RemoveInputHandler(int i)
{
    InputReceiver::instance().removeHandler(i);
}

mrAPI void UpdateInputs()
{
    InputReceiver::instance().update();
}

mrAPI IRecorder* CreateRecorder()
{
    return new Recorder();
}

} // namespace mr
