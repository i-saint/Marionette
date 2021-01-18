#include "pch.h"
#include "resource.h"
#include "MouseReplayer.h"

#define mrTPlay L"▶"
#define mrTRec  L"⚫"
#define mrTStop L"❚❚"
#define mrTExit L"✖"


class MouseReplayerApp
{
public:
    static MouseReplayerApp& instance();

    void start();
    void finish();
    bool toggleRecording();
    bool togglePlaying();
    bool exit();

    // internal
    void repaint();
    void setDataPath(const char *v);
    bool onInput(mr::OpRecord& rec);

public:
    MouseReplayerApp();
    ~MouseReplayerApp();
    MouseReplayerApp(const MouseReplayerApp&) = delete;

    HWND m_hwnd = nullptr;
    mr::IRecorderPtr m_recorder;
    mr::IPlayerPtr m_player;
    std::string m_data_path = "replay.txt";
    bool m_finished = false;

    HBRUSH m_brush_recording = nullptr;
    HBRUSH m_brush_playing = nullptr;

    std::map<mr::Key, mr::IPlayerPtr> m_keymap;
};


static void HandleClientAreaDrag(HWND hwnd, UINT msg, int mouseX, int mouseY)
{
    static int s_captureX = 0;
    static int s_captureY = 0;

    switch (msg)
    {
    case WM_LBUTTONDOWN:
        s_captureX = mouseX;
        s_captureY = mouseY;
        ::SetCapture(hwnd);
        break;

    case WM_LBUTTONUP:
        ::ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
        if (::GetCapture() == hwnd)
        {
            RECT rc;
            ::GetWindowRect(hwnd, &rc);
            int  newX = rc.left + mouseX - s_captureX;
            int  newY = rc.top + mouseY - s_captureY;
            int  width = rc.right - rc.left;
            int  height = rc.bottom - rc.top;
            UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
            ::SetWindowPos(hwnd, NULL, newX, newY, width, height, flags);
        }
        break;
    }
}

static INT_PTR CALLBACK mrDialogCB(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto GetApp = [hDlg]() -> MouseReplayerApp& {
        return MouseReplayerApp::instance();
    };
    auto CtrlSetText = [hDlg](int cid, const wchar_t* v) {
        ::SetDlgItemTextW(hDlg, cid, v);
    };
    auto CtrlEnable = [hDlg](int cid, bool v) {
        ::EnableWindow(GetDlgItem(hDlg, cid), v);
    };

    INT_PTR ret = FALSE;
    switch (msg) {
    case WM_INITDIALOG:
    {
        // .rc file can not handle unicode. non-ascii characters must be set from program.
        // https://social.msdn.microsoft.com/Forums/ja-JP/fa09ec19-0253-478b-849f-9ae2980a3251
        CtrlSetText(IDC_BUTTON_PLAY, mrTPlay);
        CtrlSetText(IDC_BUTTON_RECORDING, mrTRec);
        CtrlSetText(IDC_BUTTON_EXIT, mrTExit);
        ::ShowWindow(hDlg, SW_SHOW);
        ret = TRUE;
        break;
    }

    case WM_CLOSE:
    {
        ::DestroyWindow(hDlg);
        ret = TRUE;

        GetApp().finish();
        break;
    }

    case WM_CTLCOLORDLG:
    {
        auto& app = GetApp();
        // change background color if recording|playing
        if (app.m_player)
            return (INT_PTR)app.m_brush_playing;
        if (app.m_recorder)
            return (INT_PTR)app.m_brush_recording;
        return (INT_PTR)nullptr;
    }

    // handle drag & drop and move window
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        HandleClientAreaDrag(hDlg, WM_MOUSEMOVE, x, y);
        break;
    }
    case WM_LBUTTONUP:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        HandleClientAreaDrag(hDlg, WM_LBUTTONUP, x, y);
        break;
    }
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        HandleClientAreaDrag(hDlg, WM_LBUTTONDOWN, x, y);
        break;
    }

    // handle file drop
    case WM_DROPFILES:
    {
        auto hDrop = (HDROP)wParam;
        char path[MAX_PATH];
        ::DragQueryFileA(hDrop, 0, path, sizeof(path));
        GetApp().setDataPath(path);
        break;
    }

    // handle buttons
    case WM_COMMAND:
    {
        auto& app = GetApp();

        int code = HIWORD(wParam);
        int cid = LOWORD(wParam);
        switch (cid) {
        case IDC_BUTTON_PLAY:
        {
            app.togglePlaying();
            ret = TRUE;
            break;
        }

        case IDC_BUTTON_RECORDING:
        {
            app.toggleRecording();
            ret = TRUE;
            break;
        }

        case IDC_BUTTON_EXIT:
        {
            app.exit();
            ret = TRUE;
            break;
        }

        default:
            break;
        }
        break;
    }

    default:
        break;
    }
    return ret;
}

MouseReplayerApp& MouseReplayerApp::instance()
{
    static MouseReplayerApp s_instance;
    return s_instance;
}

void MouseReplayerApp::start()
{
    mr::LoadKeymap("keymap.txt", [this](mr::Key k, std::string path) {
        auto player = mr::CreatePlayerShared();
        if (player->load(path.c_str())) {
            player->setMatchTarget(mr::MatchTarget::ForegroundWindow);
            m_keymap[k] = player;
        }
        mrDbgPrint("%d %s\n", k.code, path.c_str());
        });

    auto receiver = mr::GetReceiver();
    receiver->addHandler([this](mr::OpRecord& rec) { return onInput(rec); });

    m_hwnd = ::CreateDialogParam(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_MAINWINDOW), nullptr, mrDialogCB, (LPARAM)this);
    m_brush_recording = CreateSolidBrush(RGB(255, 0, 0));
    m_brush_playing = CreateSolidBrush(RGB(255, 255, 0));

    MSG msg;
    for (;;) {
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        receiver->update();

        if (m_player) {
            m_player->update();

            if (!m_player->isPlaying())
                togglePlaying();
        }
        if (m_recorder) {
            m_recorder->update();
            if (!m_recorder->isRecording())
                toggleRecording();
        }

        for (auto& kvp : m_keymap)
            kvp.second->update();

        // possible better way:
        // use GetMesssage() and Player & Recorder do their jobs in worker threads.
        mr::SleepMS(1);

        if (m_finished)
            break;
    }
}

MouseReplayerApp::MouseReplayerApp()
{
}

MouseReplayerApp::~MouseReplayerApp()
{
    finish();
}

void MouseReplayerApp::finish()
{
    m_finished = true;
    if (m_brush_recording) {
        ::DeleteObject(m_brush_recording);
        m_brush_recording = nullptr;
    }
    if (m_brush_playing) {
        ::DeleteObject(m_brush_playing);
        m_brush_playing = nullptr;
    }
}

bool MouseReplayerApp::toggleRecording()
{
    if (m_recorder) {
        m_recorder->stop();
        m_recorder->save(m_data_path.c_str());
        m_recorder = nullptr;

        ::SetDlgItemTextW(m_hwnd, IDC_BUTTON_RECORDING, mrTRec);
        ::EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_PLAY), true);
        repaint();
        return false;
    }
    else {
        m_recorder = mr::CreateRecorderShared();
        m_recorder->start();

        ::SetDlgItemTextW(m_hwnd, IDC_BUTTON_RECORDING, mrTStop);
        ::EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_PLAY), false);
        repaint();
        return true;
    }
}

bool MouseReplayerApp::togglePlaying()
{
    if (m_player) {
        m_player->stop();
        m_player = nullptr;

        ::SetDlgItemTextW(m_hwnd, IDC_BUTTON_PLAY, mrTPlay);
        ::EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_RECORDING), true);
        repaint();
        return false;
    }
    else {
        m_player = mr::CreatePlayerShared();
        m_player->load(m_data_path.c_str());
        m_player->start();

        ::SetDlgItemTextW(m_hwnd, IDC_BUTTON_PLAY, mrTStop);
        ::EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_RECORDING), false);
        repaint();
        return true;
    }
}

bool MouseReplayerApp::exit()
{
    if (m_hwnd) {
        ::SendMessage(m_hwnd, WM_CLOSE, 0, 0);
        return true;
    }
    else {
        return false;
    }
}

void MouseReplayerApp::repaint()
{
    ::InvalidateRect(m_hwnd, nullptr, 1);
    ::UpdateWindow(m_hwnd);
}

void MouseReplayerApp::setDataPath(const char* v)
{
    m_data_path = v;
}

bool MouseReplayerApp::onInput(mr::OpRecord& rec)
{
    static bool s_ctrl, s_alt, s_shift;
    if (rec.type == mr::OpType::KeyDown) {
        // stop if escape is pressed
        if (rec.data.key.code == VK_ESCAPE) {
            if (m_player && m_player->isPlaying())
                m_player->stop();
            else if (m_recorder && m_recorder->isRecording())
                m_recorder->stop();
            else
                exit();
        }

        if (rec.data.key.code == VK_CONTROL)
            s_ctrl = true;
        if (rec.data.key.code == VK_MENU)
            s_alt = true;
        if (rec.data.key.code == VK_SHIFT)
            s_shift = true;

        mr::Key k{};
        k.ctrl = s_ctrl;
        k.alt = s_alt;
        k.shift = s_shift;
        k.code = rec.data.key.code;
        auto i = m_keymap.find(k);
        if (i != m_keymap.end())
            i->second->start();
    }
    if (rec.type == mr::OpType::KeyUp) {
        if (s_ctrl && rec.data.key.code == VK_F1)
            togglePlaying();
        if (s_ctrl && rec.data.key.code == VK_F2)
            toggleRecording();

        if (rec.data.key.code == VK_CONTROL)
            s_ctrl = false;
        if (rec.data.key.code == VK_MENU)
            s_alt = false;
        if (rec.data.key.code == VK_SHIFT)
            s_shift = false;
    }

    // ignore function keys
    if (rec.data.key.code == VK_ESCAPE ||
        (rec.data.key.code >= VK_F1 && rec.data.key.code <= VK_F24)) {
        return false;
    }
    return true;
}


void mrStart()
{
    auto& app = MouseReplayerApp::instance();
    if (__argc >= 2)
        app.setDataPath(__argv[1]);
    app.start();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    auto bin_path = mr::GetCurrentModuleDirectory() + "\\bin";
    ::SetDllDirectoryA(bin_path.c_str());

    mrStart();
}
