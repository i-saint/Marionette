#include "pch.h"
#include "mrInternal.h"
#include "mrScreenCapture.h"

#ifdef mrWithDesktopDuplicationAPI
namespace mr {

class DesktopDuplication : public ScreenCaptureCommon
{
public:
    DesktopDuplication();
    ~DesktopDuplication() override;
    bool valid() const;

    bool initializeDuplication(HMONITOR hmon);
    bool startCapture(HWND hwnd) override;
    bool startCapture(HMONITOR hmon) override;
    void stopCapture() override;
    bool isCapturing() const override;

    bool getFrameInternal(int timeout_ms);
    FrameInfo getFrame() override;
    FrameInfo waitNextFrame() override;

private:
    com_ptr<IDXGIOutputDuplication> m_duplication;
};


DesktopDuplication::DesktopDuplication()
{
}

DesktopDuplication::~DesktopDuplication()
{
    stopCapture();
}

bool DesktopDuplication::valid() const
{
    // todo
    return true;
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
        com_ptr<IDXGIOutput> tmp;
        if (FAILED(adapter->EnumOutputs(i, tmp.put())) || hmon == nullptr)
            break;

        tmp->GetDesc(&desc);
        if (desc.Monitor == hmon) {
            output = tmp;
            break;
        }
    }

    if (!output || FAILED(output->QueryInterface(IID_PPV_ARGS(output1.put()))))
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
        return true;
    }
    return false;
}

void DesktopDuplication::stopCapture()
{
    m_duplication = nullptr;
    m_frame_info = {};
}

bool DesktopDuplication::isCapturing() const
{
    return m_duplication != nullptr;
}

bool DesktopDuplication::getFrameInternal(int timeout_ms)
{
    if (!m_duplication)
        return false;

    bool ret = false;
    com_ptr<IDXGIResource> resource;
    DXGI_OUTDUPL_FRAME_INFO frame_info{};
    auto hr = m_duplication->AcquireNextFrame(timeout_ms, &frame_info, resource.put());
    if (SUCCEEDED(hr)) {
        uint64_t time = frame_info.LastPresentTime.QuadPart;
        if (time != 0 && m_frame_info.present_time != time) {
            com_ptr<ID3D11Texture2D> surface;
            resource->QueryInterface(IID_PPV_ARGS(surface.put()));

            if (!m_frame_info.surface) {
                D3D11_TEXTURE2D_DESC desc{};
                surface->GetDesc(&desc);
                m_frame_info.surface = Texture2D::create(desc.Width, desc.Height, GetMRFormat(desc.Format));
                m_frame_info.size = m_frame_info.surface->getSize();
            }
            m_frame_info.present_time = time;

            // ReleaseFrame() seems invalidate surface. so, need to dispatch copy at this point.
            DispatchCopy(cast(m_frame_info.surface)->ptr(), surface.get());

            ret = true;
        }
        m_duplication->ReleaseFrame();
    }
    else if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // can be continued
    }
    else {
        // DXGI_ERROR_ACCESS_LOST or other something fatal. can not be continued.
        stopCapture();
    }
    return ret;
}

IScreenCapture::FrameInfo DesktopDuplication::getFrame()
{
    // AcquireNextFrame() before the first vsync seems result empty frame. so, wait before first call.
    if (m_frame_info.present_time == 0)
        WaitVSync();

    getFrameInternal(0);
    return m_frame_info;
}

IScreenCapture::FrameInfo DesktopDuplication::waitNextFrame()
{
    WaitVSync();
    getFrameInternal(17);
    return m_frame_info;
}

IScreenCapture* CreateDesktopDuplication_()
{
    auto ret = new DesktopDuplication();
    if (!ret->valid()) {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

} // namespace mr
#endif // mrWithDesktopDuplicationAPI
