#include "pch.h"
#include "MouseReplayer.h"


#ifdef mrWithGraphicsCapture
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.Capture.Interop.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Capture;


namespace mr {

class CaptureWindow : public ICaptureWindow
{
public:
    CaptureWindow(HWND hwnd, const CaptureHandler& handler);
    CaptureWindow(HMONITOR hmon, const CaptureHandler& handler);
    ~CaptureWindow() override;

    void release() override;
    bool valid() const;

    ID3D11Device* getDevice() override;
    ID3D11DeviceContext* getDeviceContext() override;

    template<class CreateCaptureItem>
    bool initialize(const CreateCaptureItem& body);

    void onFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

private:
    com_ptr<ID3D11Device> m_device;
    com_ptr<ID3D11DeviceContext> m_context;

    IDirect3DDevice m_device_rt{ nullptr };
    Direct3D11CaptureFramePool m_frame_pool{ nullptr };
    GraphicsCaptureItem m_capture_item{ nullptr };
    GraphicsCaptureSession m_capture_session{ nullptr };
    Direct3D11CaptureFramePool::FrameArrived_revoker m_frame_arrived;

    CaptureHandler m_handler;
};


mrAPI void InitializeCaptureWindow()
{
    static std::once_flag s_once;
    std::call_once(s_once, []() {
        init_apartment(apartment_type::single_threaded);
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


template<class CreateCaptureItem>
bool CaptureWindow::initialize(const CreateCaptureItem& cci)
{
    if (!IsGraphicsCaptureSupported())
        return false;

    InitializeCaptureWindow();

    try {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        check_hresult(::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, m_device.put(), nullptr, nullptr));
        m_device->GetImmediateContext(m_context.put());

        auto dxgi = m_device.as<IDXGIDevice>();
        com_ptr<::IInspectable> device;
        check_hresult(::CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), device.put()));
        m_device_rt = device.as<IDirect3DDevice>();

        auto factory = get_activation_factory<GraphicsCaptureItem>();
        if (auto interop = factory.as<IGraphicsCaptureItemInterop>()) {
            cci(interop);
            if (m_capture_item) {
                m_frame_pool = Direct3D11CaptureFramePool::Create(m_device_rt, DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, m_capture_item.Size());
                m_frame_arrived = m_frame_pool.FrameArrived(auto_revoke, { this, &CaptureWindow::onFrameArrived });
                m_capture_session = m_frame_pool.CreateCaptureSession(m_capture_item);
                m_capture_session.StartCapture();
                return true;
            }
        }
    }
    catch (const hresult_error& e) {
        DbgPrint(L"*** CaptureWindow raised exception: %s ***\n", e.message().c_str());
    }
    return false;
}

CaptureWindow::CaptureWindow(HWND hwnd, const CaptureHandler& handler)
{
    m_handler = handler;
    initialize([this, hwnd](auto& interop) {
        check_hresult(interop->CreateForWindow(hwnd,
            guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            reinterpret_cast<void**>(put_abi(m_capture_item))));
        });
}

CaptureWindow::CaptureWindow(HMONITOR hmon, const CaptureHandler& handler)
{
    m_handler = handler;
    initialize([this, hmon](auto& interop) {
        check_hresult(interop->CreateForMonitor(hmon,
            guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            reinterpret_cast<void**>(put_abi(m_capture_item))));
        });
}

CaptureWindow::~CaptureWindow()
{
    m_frame_arrived = {};
    m_capture_session = nullptr;
    m_frame_pool = nullptr;
}

void CaptureWindow::release()
{
    delete this;
}

bool CaptureWindow::valid() const
{
    return m_capture_session != nullptr;
}

ID3D11Device* CaptureWindow::getDevice()
{
    return m_device.get();
}

ID3D11DeviceContext* CaptureWindow::getDeviceContext()
{
    return m_context.get();
}

void CaptureWindow::onFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args)
{
    auto frame = sender.TryGetNextFrame();
    auto surface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
    m_handler(surface.get());
}

mrAPI bool IsGraphicsCaptureSupported()
{
    return GraphicsCaptureSession::IsSupported();
}


template<class T>
static ICaptureWindow* CreateCaptureWindowImpl(T a, const CaptureHandler& handler)
{
    auto ret = new CaptureWindow(a, handler);
    if (!ret->valid()) {
        delete ret;
        ret = nullptr;
    }
    return ret;
}

mrAPI ICaptureWindow* CreateCaptureWindow(HWND hwnd, const CaptureHandler& handler)
{
    return CreateCaptureWindowImpl(hwnd, handler);
}

mrAPI ICaptureWindow* CreateCaptureMonitor(HMONITOR hmon, const CaptureHandler& handler)
{
    return CreateCaptureWindowImpl(hmon, handler);
}

} // namespace mr
#endif // mrWithGraphicsCapture
