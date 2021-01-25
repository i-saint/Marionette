#include "pch.h"
#include "Internal.h"
#include "ScreenCapture.h"

#ifdef mrWithDesktopDuplicationAPI
namespace mr {

class DesktopDuplication : public RefCount<IScreenCapture>
{
public:
    ~DesktopDuplication() override;

    bool valid() const override;

    bool initializeDuplication(HMONITOR hmon);
    bool startCapture(HWND hwnd) override;
    bool startCapture(HMONITOR hmon) override;
    void stopCapture() override;
    FrameInfo getFrame() override;

    void setOnFrameArrived(const Callback& cb) override;

    // called from capture thread
    void captureLoop();
    bool getFrameInternal(int timeout_ms, com_ptr<ID3D11Texture2D>& surface, uint64_t& time);

private:
    Callback m_callback;
    FrameInfo m_frame_info;

    com_ptr<IDXGIOutputDuplication> m_duplication;
    std::atomic_bool m_end_flag = false;
    std::thread m_capture_thread;
    std::mutex m_mutex;
};


DesktopDuplication::~DesktopDuplication()
{
    stopCapture();
}

bool DesktopDuplication::valid() const
{
    return m_duplication != nullptr;
}

bool DesktopDuplication::initializeDuplication(HMONITOR hmon)
{
    stopCapture();

    mrProfile("DesktopDuplication::start");
    auto device = mrGfxDevice();

    // create duplication
    com_ptr<IDXGIDevice> dxgi;
    com_ptr<IDXGIAdapter> adapter;
    com_ptr<IDXGIOutput> output;
    com_ptr<IDXGIOutput1> output1;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(dxgi.put()))))
        return false;
    if (FAILED(dxgi->GetParent(IID_PPV_ARGS(adapter.put()))))
        return false;

    // find output by hmon
    DXGI_OUTPUT_DESC desc{};
    for (int i = 0; ; ++i) {
        if (FAILED(adapter->EnumOutputs(i, output.put())))
            return false;

        if (hmon == nullptr)
            break;
        output->GetDesc(&desc);
        if (desc.Monitor == hmon)
            break;
    }

    if (FAILED(output->QueryInterface(IID_PPV_ARGS(output1.put()))))
        return false;
    if (FAILED(output1->DuplicateOutput(device, m_duplication.put())))
        return false;
    return true;
}

bool DesktopDuplication::startCapture(HWND hwnd)
{
    return false;
}

bool DesktopDuplication::startCapture(HMONITOR hmon)
{
    if (initializeDuplication(hmon)) {
        m_capture_thread = std::thread([this]() { captureLoop(); });
        return true;
    }
    return false;
}

void DesktopDuplication::stopCapture()
{
    if (m_capture_thread.joinable()) {
        m_end_flag = true;
        m_capture_thread.join();
        m_capture_thread = {};
        m_end_flag = false;
    }
    m_duplication = nullptr;
}

DesktopDuplication::FrameInfo DesktopDuplication::getFrame()
{
    FrameInfo ret;
    {
        std::unique_lock l(m_mutex);
        ret = m_frame_info;
    }
    return ret;
}

void DesktopDuplication::setOnFrameArrived(const Callback& cb)
{
    std::unique_lock l(m_mutex);
    m_callback = cb;
}


bool DesktopDuplication::getFrameInternal(int timeout_ms, com_ptr<ID3D11Texture2D>& surface, uint64_t& time)
{
    if (!m_duplication)
        return false;

    bool ret = false;
    com_ptr<IDXGIResource> resource;
    DXGI_OUTDUPL_FRAME_INFO frame_info{};
    auto hr = m_duplication->AcquireNextFrame(timeout_ms, &frame_info, resource.put());
    if (SUCCEEDED(hr)) {
        if (frame_info.LastPresentTime.QuadPart != 0) {
            resource->QueryInterface(IID_PPV_ARGS(surface.put()));
            time = frame_info.LastPresentTime.QuadPart;
            ret = true;
        }
        m_duplication->ReleaseFrame();
    }
    else if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // can be continued
    }
    else {
        // DXGI_ERROR_ACCESS_LOST or other something fatal. can not be continued.
        m_duplication = nullptr;
    }
    return ret;
}

void DesktopDuplication::captureLoop()
{
    const int kTimeout = 30; // in ms

    while (valid() && !m_end_flag) {
        com_ptr<ID3D11Texture2D> surface{};
        uint64_t time{};
        if (getFrameInternal(kTimeout, surface, time)) {
            FrameInfo tmp{Texture2D::wrap(surface), time};
            {
                std::unique_lock l(m_mutex);
                m_frame_info = tmp;

                if (m_callback)
                    m_callback(m_frame_info);
            }
        }
    }
}

IScreenCapture* CreateDesktopDuplication_()
{
    return new DesktopDuplication();
}

} // namespace mr
#endif // mrWithDesktopDuplicationAPI
