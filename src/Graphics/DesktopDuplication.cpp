#include "pch.h"
#include "Internal.h"
#include "ScreenCapture.h"

#ifdef mrWithDesktopDuplicationAPI
namespace mr {

class DesktopDuplication : public IScreenCapture
{
public:
    ~DesktopDuplication() override;

    void release() override;
    bool valid() const override;
    FrameInfo getFrame();

    bool start(HMONITOR hmon);
    void stop();

    using Callback = std::function<void(com_ptr<ID3D11Texture2D>& surface, uint64_t time)>;
    bool getFrame(int timeout_ms, const Callback& calback);

    // capture thread
    void captureLoop();

private:
    FrameInfo m_frame_info;

    com_ptr<IDXGIOutputDuplication> m_duplication;
    bool m_end_flag = false;
    std::thread m_capture_thread;
    std::mutex m_mutex;
};


DesktopDuplication::~DesktopDuplication()
{
    stop();
}

void DesktopDuplication::release()
{
    delete this;
}

bool DesktopDuplication::valid() const
{
    return m_duplication != nullptr;
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

bool DesktopDuplication::start(HMONITOR hmon)
{
    stop();

    mrProfile("DesktopDuplication::start");
    auto device = mrGetDevice();

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

    m_capture_thread = std::thread([this]() { captureLoop(); });
    return true;
}

void DesktopDuplication::stop()
{
    m_end_flag = true;
    if (m_capture_thread.joinable()) {
        m_capture_thread.join();
        m_capture_thread = {};
    }
    m_duplication = nullptr;
}

bool DesktopDuplication::getFrame(int timeout_ms, const Callback& calback)
{
    if (!m_duplication)
        return false;

    bool ret = false;
    com_ptr<IDXGIResource> resource;
    DXGI_OUTDUPL_FRAME_INFO frame_info{};
    auto hr = m_duplication->AcquireNextFrame(timeout_ms, &frame_info, resource.put());
    if (SUCCEEDED(hr)) {
        if (frame_info.LastPresentTime.QuadPart != 0) {
            com_ptr<ID3D11Texture2D> surface;
            resource->QueryInterface(IID_PPV_ARGS(surface.put()));

            calback(surface, (uint64_t)frame_info.LastPresentTime.QuadPart);
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
        getFrame(kTimeout, [this](com_ptr<ID3D11Texture2D>& surface, uint64_t time) {
            FrameInfo tmp{Texture2D::wrap(surface), time};
            {
                std::unique_lock l(m_mutex);
                m_frame_info = tmp;
            }
            });
    }
}

IScreenCapture* CreateDesktopDuplication()
{
    return new DesktopDuplication();
}

} // namespace mr
#endif // mrWithDesktopDuplicationAPI
