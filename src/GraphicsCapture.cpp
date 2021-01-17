#include "pch.h"
#include "MouseReplayer.h"


#ifdef mrWithGraphicsCapture
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.Capture.Interop.h>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Capture;

namespace mr {

static const DWORD kTimeoutMS = 3000;

// thin wrapper for Windows' event
class FenceEvent
{
public:
    FenceEvent();
    FenceEvent(const FenceEvent& v);
    FenceEvent& operator=(const FenceEvent& v);
    ~FenceEvent();
    operator HANDLE() const;

private:
    HANDLE m_handle = nullptr;
};

class CaptureWindow : public ICaptureWindow
{
public:
    CaptureWindow();
    ~CaptureWindow() override;

    bool valid() const;
    void release() override;
    bool start(HWND hwnd, const CaptureHandler& handler) override;
    bool start(HMONITOR hmon, const CaptureHandler& handler) override;
    void stop() override;


    ID3D11Device* getDevice() override;
    ID3D11DeviceContext* getDeviceContext() override;
    bool getPixels(ID3D11Texture2D* tex, const PixelHandler& handler) override;

    template<class CreateCaptureItem>
    bool initialize(const CreateCaptureItem& body);

    void onFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

private:
    com_ptr<ID3D11Device5> m_device;
    com_ptr<ID3D11DeviceContext4> m_context;
    com_ptr<ID3D11Fence> m_fence;
    com_ptr<ID3D11Texture2D> m_buffer;
    FenceEvent m_fence_event;
    uint64_t m_fence_value = 0;

    IDirect3DDevice m_device_rt{ nullptr };
    Direct3D11CaptureFramePool m_frame_pool{ nullptr };
    GraphicsCaptureItem m_capture_item{ nullptr };
    GraphicsCaptureSession m_capture_session{ nullptr };
    Direct3D11CaptureFramePool::FrameArrived_revoker m_frame_arrived;

    CaptureHandler m_handler;
};


FenceEvent::FenceEvent()
{
    m_handle = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

FenceEvent::FenceEvent(const FenceEvent& v)
{
    *this = v;
}

FenceEvent& FenceEvent::operator=(const FenceEvent& v)
{
    ::DuplicateHandle(::GetCurrentProcess(), v.m_handle, ::GetCurrentProcess(), &m_handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
    return *this;
}

FenceEvent::~FenceEvent()
{
    ::CloseHandle(m_handle);
}

FenceEvent::operator HANDLE() const
{
    return m_handle;
}


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
    auto createStagingTexture = [this](UINT width, UINT height) {
        D3D11_TEXTURE2D_DESC desc = {
            width, height, 1, 1, DXGI_FORMAT_B8G8R8A8_TYPELESS, { 1, 0 },
            D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0
        };
        com_ptr<ID3D11Texture2D> ret;
        check_hresult(m_device->CreateTexture2D(&desc, nullptr, ret.put()));
        return ret;
    };

    try {
        {
            UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef mrDebug
            flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
            com_ptr<ID3D11Device> device;
            check_hresult(::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, nullptr));
            device->QueryInterface(IID_PPV_ARGS(&m_device));

            m_device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
        }
        {
            com_ptr<ID3D11DeviceContext> context;
            m_device->GetImmediateContext(context.put());
            context->QueryInterface(IID_PPV_ARGS(&m_context));
        }
        {
            auto dxgi = m_device.as<IDXGIDevice>();
            com_ptr<::IInspectable> device;
            check_hresult(::CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), device.put()));
            m_device_rt = device.as<IDirect3DDevice>();
        }

        auto factory = get_activation_factory<GraphicsCaptureItem>();
        if (auto interop = factory.as<IGraphicsCaptureItemInterop>()) {
            cci(interop);
            if (m_capture_item) {
                auto size = m_capture_item.Size();
                m_buffer = createStagingTexture(size.Width, size.Height);
                m_frame_pool = Direct3D11CaptureFramePool::Create(m_device_rt, DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, size);
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

CaptureWindow::CaptureWindow()
{
    InitializeCaptureWindow();
}

CaptureWindow::~CaptureWindow()
{
    stop();
}

bool CaptureWindow::valid() const
{
    return m_capture_session != nullptr;
}

void CaptureWindow::release()
{
    delete this;
}

bool CaptureWindow::start(HWND hwnd, const CaptureHandler& handler)
{
    stop();

    m_handler = handler;
    initialize([this, hwnd](auto& interop) {
        check_hresult(interop->CreateForWindow(hwnd,
            guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            reinterpret_cast<void**>(put_abi(m_capture_item))));
        });
    return valid();
}

bool CaptureWindow::start(HMONITOR hmon, const CaptureHandler& handler)
{
    stop();

    m_handler = handler;
    initialize([this, hmon](auto& interop) {
        check_hresult(interop->CreateForMonitor(hmon,
            guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
            reinterpret_cast<void**>(put_abi(m_capture_item))));
        });
    return valid();
}

void CaptureWindow::stop()
{
    m_frame_arrived.revoke();
    m_capture_session = nullptr;

    if (m_frame_pool) {
        m_frame_pool.Close();
        m_frame_pool = nullptr;
    }
}

ID3D11Device* CaptureWindow::getDevice()
{
    return m_device.get();
}

ID3D11DeviceContext* CaptureWindow::getDeviceContext()
{
    return m_context.get();
}

bool CaptureWindow::getPixels(ID3D11Texture2D* tex, const PixelHandler& handler)
{
    if (!tex)
        return false;

    // dispatch copy
    m_context->CopyResource(m_buffer.get(), tex);

    // wait for completion of copy
    uint64_t fv = ++m_fence_value;
    m_context->Signal(m_fence.get(), fv);
    m_context->Flush();
    m_fence->SetEventOnCompletion(fv, m_fence_event);
    ::WaitForSingleObject(m_fence_event, kTimeoutMS);

    D3D11_TEXTURE2D_DESC desc{};
    tex->GetDesc(&desc);

    // map & unmap
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_context->Map(m_buffer.get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr)) {
        handler((const byte*)mapped.pData, desc.Width, desc.Height, mapped.RowPitch);
        m_context->Unmap(m_buffer.get(), 0);
        return true;
    }
    return false;
}

void CaptureWindow::onFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args)
{
    try {
        auto frame = sender.TryGetNextFrame();
        auto surface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
        m_handler(surface.get());
        frame.Close();
    }
    catch (const hresult_error& e) {
        DbgPrint(L"*** CaptureWindow raised exception: %s ***\n", e.message().c_str());
    }
}

mrAPI bool IsGraphicsCaptureSupported()
{
    return GraphicsCaptureSession::IsSupported();
}

mrAPI ICaptureWindow* CreateCaptureWindow()
{
    if (!IsGraphicsCaptureSupported())
        return nullptr;
    return new CaptureWindow();
}

} // namespace mr
#endif // mrWithGraphicsCapture
