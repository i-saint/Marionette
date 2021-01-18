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

class GraphicsCapture : public IGraphicsCapture
{
public:
    GraphicsCapture();
    ~GraphicsCapture() override;

    bool valid() const;
    void release() override;
    void setOptions(const Options& opt) override;
    bool start(HWND hwnd, const CaptureHandler& handler) override;
    bool start(HMONITOR hmon, const CaptureHandler& handler) override;
    void stop() override;


    ID3D11Device* getDevice() override;
    ID3D11DeviceContext* getDeviceContext() override;
    bool getPixels(const PixelHandler& handler) override;

    template<class CreateCaptureItem>
    bool startImpl(const CreateCaptureItem& body);

    void onFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

private:
    Options m_options;

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


mrAPI void InitializeGraphicsCapture()
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

GraphicsCapture::GraphicsCapture()
{
    InitializeGraphicsCapture();
}

GraphicsCapture::~GraphicsCapture()
{
    stop();
}

bool GraphicsCapture::valid() const
{
    return m_capture_session != nullptr;
}

void GraphicsCapture::release()
{
    delete this;
}

void GraphicsCapture::setOptions(const Options& opt)
{
    m_options = opt;
}

template<class CreateCaptureItem>
bool GraphicsCapture::startImpl(const CreateCaptureItem& cci)
{
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
                if (m_options.free_threaded)
                    m_frame_pool = Direct3D11CaptureFramePool::CreateFreeThreaded(
                        m_device_rt, DirectXPixelFormat::B8G8R8A8UIntNormalized, m_options.buffer_count, size);
                else
                    m_frame_pool = Direct3D11CaptureFramePool::Create(
                        m_device_rt, DirectXPixelFormat::B8G8R8A8UIntNormalized, m_options.buffer_count, size);
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

bool GraphicsCapture::start(HWND hwnd, const CaptureHandler& handler)
{
    stop();

    m_handler = handler;
    startImpl([this, hwnd](auto& interop) {
        check_hresult(interop->CreateForWindow(hwnd, guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), put_abi(m_capture_item)));
        });
    return valid();
}

bool GraphicsCapture::start(HMONITOR hmon, const CaptureHandler& handler)
{
    stop();

    m_handler = handler;
    startImpl([this, hmon](auto& interop) {
        check_hresult(interop->CreateForMonitor(hmon, guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), put_abi(m_capture_item)));
        });
    return valid();
}

void GraphicsCapture::stop()
{
    m_frame_arrived.revoke();
    m_capture_session = nullptr;
    if (m_frame_pool) {
        m_frame_pool.Close();
        m_frame_pool = nullptr;
    }
    m_buffer = nullptr;
}

ID3D11Device* GraphicsCapture::getDevice()
{
    return m_device.get();
}

ID3D11DeviceContext* GraphicsCapture::getDeviceContext()
{
    return m_context.get();
}

bool GraphicsCapture::getPixels(const PixelHandler& handler)
{
    if (!m_buffer || !m_options.cpu_readable) {
        mrDbgPrint("GraphicsCapture::getPixels() require create_backbuffer and cpu_read option\n");
        return false;
    }

    {
        mrProfile("GraphicsCapture: copy texture (wait)");

        // wait for completion
        uint64_t fv = ++m_fence_value;
        m_context->Signal(m_fence.get(), fv);
        m_context->Flush();
        m_fence->SetEventOnCompletion(fv, m_fence_event);
        ::WaitForSingleObject(m_fence_event, kTimeoutMS);
    }

    {
        mrProfile("GraphicsCapture: map texture");

        // map & unmap
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = m_context->Map(m_buffer.get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (SUCCEEDED(hr)) {
            D3D11_TEXTURE2D_DESC desc{};
            m_buffer->GetDesc(&desc);

            handler((const byte*)mapped.pData, desc.Width, desc.Height, mapped.RowPitch);
            m_context->Unmap(m_buffer.get(), 0);
            return true;
        }
    }
    return false;
}

void GraphicsCapture::onFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args)
{
    auto createStagingTexture = [this](UINT width, UINT height) {
        D3D11_TEXTURE2D_DESC desc = {
            width, height, 1, 1, DXGI_FORMAT_B8G8R8A8_TYPELESS, { 1, 0 },
            D3D11_USAGE_STAGING, 0, 0, 0
        };
        if (m_options.cpu_readable)
            desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;

        com_ptr<ID3D11Texture2D> ret;
        check_hresult(m_device->CreateTexture2D(&desc, nullptr, ret.put()));
        return ret;
    };

    try {
        auto frame = sender.TryGetNextFrame();
        auto size = frame.ContentSize();
        auto surface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

        if (m_options.create_backbuffer) {
            // create staging texture if needed
            if (m_buffer) {
                D3D11_TEXTURE2D_DESC desc{};
                m_buffer->GetDesc(&desc);
                if (desc.Width != size.Width || desc.Height != size.Height)
                    m_buffer = {};
            }
            if (!m_buffer) {
                m_buffer = createStagingTexture(size.Width, size.Height);
            }

            // dispatch copy
            // size of frame.Surface() and frame.ContentSize() may mismatch. so CopySubresourceRegion() to partial copy.
            {
                D3D11_BOX box{};
                box.right = size.Width;
                box.bottom = size.Height;
                box.back = 1;
                m_context->CopySubresourceRegion(m_buffer.get(), 0, 0, 0, 0, surface.get(), 0, &box);
            }

            // call handler
            m_handler(m_buffer.get());
        }
        else {
            m_handler(surface.get());
        }

        frame.Close();
    }
    catch (const hresult_error& e) {
        mrDbgPrint(L"*** GraphicsCapture raised exception: %s ***\n", e.message().c_str());
    }
}

mrAPI bool IsGraphicsCaptureSupported()
{
    return GraphicsCaptureSession::IsSupported();
}

mrAPI IGraphicsCapture* CreateGraphicsCapture()
{
    if (!IsGraphicsCaptureSupported())
        return nullptr;
    return new GraphicsCapture();
}

} // namespace mr
#endif // mrWithGraphicsCapture
