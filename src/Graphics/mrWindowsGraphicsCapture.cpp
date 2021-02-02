#include "pch.h"
#include "mrInternal.h"
#include "mrScreenCapture.h"

#ifdef mrWithWindowsGraphicsCapture
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.Capture.Interop.h>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "dwmapi.lib")

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Capture;

namespace mr {

class GraphicsCapture : public ScreenCaptureCommon
{
public:
    GraphicsCapture();
    ~GraphicsCapture() override;
    bool valid() const;

    template<class CreateCaptureItem>
    bool startImpl(const CreateCaptureItem& body);

    bool startCapture(HWND hwnd) override;
    bool startCapture(HMONITOR hmon) override;
    void stopCapture() override;
    bool isCapturing() const override;

    // called from capture thread
    void onFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

private:
    IDirect3DDevice m_device_rt{ nullptr };
    Direct3D11CaptureFramePool m_frame_pool{ nullptr };
    GraphicsCaptureItem m_capture_item{ nullptr };
    GraphicsCaptureSession m_capture_session{ nullptr };
    Direct3D11CaptureFramePool::FrameArrived_revoker m_frame_arrived;
};


static void InitializeGraphicsCapture()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        init_apartment();
        });
}


template <typename T>
inline auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
    auto access = object.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    com_ptr<T> result;
    check_hresult(access->GetInterface(guid_of<T>(), result.put_void()));
    return result;
}

GraphicsCapture::GraphicsCapture()
{
    InitializeGraphicsCapture();

    auto dxgi = As<IDXGIDevice>(mrGfxDevice());
    com_ptr<::IInspectable> device_rt;
    ::CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), device_rt.put());
    m_device_rt = device_rt.as<IDirect3DDevice>();
}

GraphicsCapture::~GraphicsCapture()
{
    stopCapture();
}

bool GraphicsCapture::valid() const
{
    return m_device_rt != nullptr;
}


template<class CreateCaptureItem>
bool GraphicsCapture::startImpl(const CreateCaptureItem& cci)
{
    stopCapture();

    mrProfile("GraphicsCapture::start()");
    try {
        auto factory = get_activation_factory<GraphicsCaptureItem>();
        if (auto interop = factory.as<IGraphicsCaptureItemInterop>()) {
            cci(interop);
            if (m_capture_item) {
                auto size = m_capture_item.Size();
                m_frame_pool = Direct3D11CaptureFramePool::CreateFreeThreaded(
                    m_device_rt, DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, size);
                m_frame_arrived = m_frame_pool.FrameArrived(auto_revoke, { this, &GraphicsCapture::onFrameArrived });
                m_capture_session = m_frame_pool.CreateCaptureSession(m_capture_item);
                m_capture_session.StartCapture();
                return true;
            }
        }
    }
    catch (const hresult_error& e) {
        mrDbgPrint(L"*** GraphicsCapture raised exception: %s ***\n", e.message().c_str());
    }
    return false;
}

bool GraphicsCapture::startCapture(HWND hwnd)
{
    startImpl([this, hwnd](auto& interop) {
        check_hresult(interop->CreateForWindow(hwnd, guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), put_abi(m_capture_item)));
        });
    return valid();
}

bool GraphicsCapture::startCapture(HMONITOR hmon)
{
    startImpl([this, hmon](auto& interop) {
        check_hresult(interop->CreateForMonitor(hmon, guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), put_abi(m_capture_item)));
        });
    return valid();
}

void GraphicsCapture::stopCapture()
{
    m_frame_arrived.revoke();
    m_capture_session = nullptr;
    if (m_frame_pool) {
        m_frame_pool.Close();
        m_frame_pool = nullptr;
    }
}

bool GraphicsCapture::isCapturing() const
{
    return m_frame_pool != nullptr;
}

void GraphicsCapture::onFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args)
{
    try {
        auto frame = sender.TryGetNextFrame();
        auto size = frame.ContentSize();
        auto surface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

        updateFrame(surface, { size.Width, size.Height }, NowNS());

        frame.Close();
    }
    catch (const hresult_error& e) {
        mrDbgPrint(L"*** GraphicsCapture raised exception: %s ***\n", e.message().c_str());
    }
}

bool IsWindowsGraphicsCaptureSupported()
{
    return GraphicsCaptureSession::IsSupported();
}

IScreenCapture* CreateGraphicsCapture_()
{
    if (!IsWindowsGraphicsCaptureSupported())
        return nullptr;

    auto ret = new GraphicsCapture();
    if (!ret->valid()) {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

} // namespace mr
#endif // mrWithWindowsGraphicsCapture
