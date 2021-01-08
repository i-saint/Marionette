#include "pch.h"
#include "resource.h"
#include "MouseReplayer.h"


class MouseReplayerApp
{
public:
    void start();
    void finish();
    bool toggleRecording();
    bool togglePlaying();

    bool onRecord(mr::OpRecord& rec);

public:
    HWND m_hwnd = nullptr;
    mr::IRecorderPtr m_recorder;
    mr::IPlayerPtr m_player;
    std::string m_data_path = "replay.txt";
    bool m_finished = false;
};

static INT_PTR CALLBACK mrDialogCB(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto GetApp = [hDlg]() {
        return (MouseReplayerApp*)::GetWindowLongPtr(hDlg, GWLP_USERDATA);
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
        CtrlSetText(IDC_BUTTON_PLAY, L"▶");
        CtrlSetText(IDC_BUTTON_RECORDING, L"●");
        ::SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
        ::ShowWindow(hDlg, SW_SHOW);
        ret = TRUE;
        break;
    }

    case WM_CLOSE:
    {
        DestroyWindow(hDlg);
        ret = TRUE;

        auto app = GetApp();
        app->finish();
        break;
    }

    case WM_COMMAND:
    {
        auto app = GetApp();

        int code = HIWORD(wParam);
        int cid = LOWORD(wParam);
        switch (cid) {
        case IDC_BUTTON_PLAY:
        {
            app->togglePlaying();
            ret = TRUE;
            break;
        }

        case IDC_BUTTON_RECORDING:
        {
            app->toggleRecording();
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

void MouseReplayerApp::start()
{
    m_hwnd = ::CreateDialogParam(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_MAINWINDOW), nullptr, mrDialogCB, (LPARAM)this);

    MSG msg;
    for (;;) {
        while (::PeekMessage(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        if (m_player) {
            m_player->update();

            // stop if escape key is pressed
            if (::GetKeyState(VK_ESCAPE) & 0x80)
                m_player->stop();

            if (!m_player->isPlaying())
                togglePlaying();
        }

        if (m_recorder) {
            m_recorder->update();
            if (!m_recorder->isRecording())
                toggleRecording();
        }

        if (m_finished)
            break;
    }
}

void MouseReplayerApp::finish()
{
    m_finished = true;
}

bool MouseReplayerApp::toggleRecording()
{
    if (m_recorder) {
        m_recorder->stop();
        m_recorder->save(m_data_path.c_str());
        m_recorder = nullptr;

        ::SetDlgItemTextW(m_hwnd, IDC_BUTTON_RECORDING, L"●");
        ::EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_PLAY), true);
        return false;
    }
    else {
        m_recorder = mr::CreateRecorderShared();
        m_recorder->setHandler([this](mr::OpRecord& rec) { return onRecord(rec); });
        m_recorder->start();

        ::SetDlgItemTextW(m_hwnd, IDC_BUTTON_RECORDING, L"‖");
        ::EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_PLAY), false);
        return true;
    }
}

bool MouseReplayerApp::togglePlaying()
{
    if (m_player) {
        m_player->stop();
        m_player = nullptr;

        ::SetDlgItemTextW(m_hwnd, IDC_BUTTON_PLAY, L"▶");
        ::EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_RECORDING), true);
        return false;
    }
    else {
        m_player = mr::CreatePlayerShared();
        m_player->load(m_data_path.c_str());
        m_player->start();

        ::SetDlgItemTextW(m_hwnd, IDC_BUTTON_PLAY, L"‖");
        ::EnableWindow(GetDlgItem(m_hwnd, IDC_BUTTON_RECORDING), false);
        return true;
    }
}

bool MouseReplayerApp::onRecord(mr::OpRecord& rec)
{
    if (rec.type == mr::OpType::KeyDown) {
        if (rec.data.key.code == VK_ESCAPE) {
            mr::OpRecord rec;
            rec.type = mr::OpType::Wait;
            m_recorder->addRecord(rec);
            m_recorder->stop();
            return false;
        }
    }
    return true;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    auto app = std::make_shared<MouseReplayerApp>();
    app->start();
}
