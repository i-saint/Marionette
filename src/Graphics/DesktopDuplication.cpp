#include "pch.h"
#include "Internal.h"
#include "GfxFoundation.h"

#ifdef mrWithDesktopDuplicationAPI
namespace mr {

class DesktopDuplication : public IDesktopDuplication
{
public:
    ~DesktopDuplication() override;
    void release() override;
    bool start(HMONITOR hmon) override;
    void stop() override;

    bool getFrame(int timeout_ms, const Callback& calback) override;

private:
    com_ptr<IDXGIOutputDuplication> m_duplication;
};


void DesktopDuplication::stop()
{
    m_duplication = nullptr;
}

DesktopDuplication::~DesktopDuplication()
{
    stop();
}

void DesktopDuplication::release()
{
    delete this;
}

bool DesktopDuplication::start(HMONITOR hmon)
{
    mrProfile("DesktopDuplication::start");
    // create device
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
    return true;
}

bool DesktopDuplication::getFrame(int timeout_ms, const Callback& calback)
{
    if (!m_duplication)
        return false;

    mrProfile("DesktopDuplication::getFrame");
    bool ret = false;
    com_ptr<IDXGIResource> resource;
    DXGI_OUTDUPL_FRAME_INFO frame_info{};
    if (SUCCEEDED(m_duplication->AcquireNextFrame(timeout_ms, &frame_info, resource.put()))) {
        if (frame_info.LastPresentTime.QuadPart != 0) {
            com_ptr<ID3D11Texture2D> surface;
            resource->QueryInterface(IID_PPV_ARGS(surface.put()));

            DXGI_OUTDUPL_DESC desc;
            m_duplication->GetDesc(&desc);

            calback(surface.get(), desc.ModeDesc.Width, desc.ModeDesc.Height);
            ret = true;
        }
        m_duplication->ReleaseFrame();
    }
    return ret;
}

IDesktopDuplication* CreateDesktopDuplication()
{
    return new DesktopDuplication();
}

} // namespace mr
#endif // mrWithDesktopDuplicationAPI
