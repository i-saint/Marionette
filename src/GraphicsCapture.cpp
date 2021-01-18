#include "pch.h"
#include "MouseReplayer.h"


#ifdef mrWithGraphicsCapture
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.Capture.Interop.h>

// shader binaries
#include "copy.hlsl.h"

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
    com_ptr<ID3D11Buffer> m_constants;
    com_ptr<ID3D11Texture2D> m_framebuffer;
    com_ptr<ID3D11Texture2D> m_stagingbuffer;

    com_ptr<ID3D11ShaderResourceView> m_srv_surface;
    com_ptr<ID3D11UnorderedAccessView> m_uav_framebuffer;
    com_ptr<ID3D11SamplerState> m_sampler;
    com_ptr<ID3D11ComputeShader> m_cs_copy;

    FenceEvent m_fence_event;
    uint64_t m_fence_value = 0;
    int m_fb_width = 0;
    int m_fb_height = 0;

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

    m_srv_surface = nullptr;
    m_uav_framebuffer = nullptr;

    m_framebuffer = nullptr;
    m_stagingbuffer = nullptr;
    m_constants = nullptr;
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
    if (!m_stagingbuffer) {
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
        HRESULT hr = m_context->Map(m_stagingbuffer.get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (SUCCEEDED(hr)) {
            D3D11_TEXTURE2D_DESC desc{};
            m_stagingbuffer->GetDesc(&desc);

            handler((const byte*)mapped.pData, desc.Width, desc.Height, mapped.RowPitch);
            m_context->Unmap(m_stagingbuffer.get(), 0);
            return true;
        }
    }
    return false;
}

void GraphicsCapture::onFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args)
{
    struct CopyParams
    {
        float pixel_width;
        float pixel_height;
        int grayscale;
        int pad;
    };

    auto createFrameBuffer = [this](UINT width, UINT height) {
        DXGI_FORMAT format = m_options.grayscale ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
        {
            D3D11_TEXTURE2D_DESC desc{ width, height, 1, 1, format, { 1, 0 }, D3D11_USAGE_DEFAULT, 0, 0, 0 };
            desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            check_hresult(m_device->CreateTexture2D(&desc, nullptr, m_framebuffer.put()));
        }
        if (m_options.cpu_readable) {
            D3D11_TEXTURE2D_DESC desc{ width, height, 1, 1, format, { 1, 0 }, D3D11_USAGE_STAGING, 0, 0, 0 };
            desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ;
            check_hresult(m_device->CreateTexture2D(&desc, nullptr, m_stagingbuffer.put()));
        }
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = format;
            desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipSlice = 0;
            check_hresult(m_device->CreateUnorderedAccessView(m_framebuffer.get(), &desc, m_uav_framebuffer.put()));
        }
    };

    auto createConstantBuffer = [this](const CopyParams& p) {
        {
            D3D11_BUFFER_DESC desc{ sizeof(p), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, sizeof(p) };
            D3D11_SUBRESOURCE_DATA data{ &p, sizeof(p), sizeof(p) };
            check_hresult(m_device->CreateBuffer(&desc, &data, m_constants.put()));
        }
    };

    auto createSurfaceSRV = [this](ID3D11Texture2D* surf) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        check_hresult(m_device->CreateShaderResourceView(surf, &desc, m_srv_surface.put()));
    };

    auto setup = [this]() {
        {
            D3D11_SAMPLER_DESC desc{};
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            check_hresult(m_device->CreateSamplerState(&desc, m_sampler.put()));
        }
        {
            check_hresult(m_device->CreateComputeShader(g_copy, std::size(g_copy), nullptr, m_cs_copy.put()));
        }
    };

    try {
        auto frame = sender.TryGetNextFrame();
        auto size = frame.ContentSize();
        auto surface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

        if (m_options.create_backbuffer) {
            if (!m_cs_copy) {
                setup();
            }
            createSurfaceSRV(surface.get());

            // create staging texture if needed
            if (!m_framebuffer) {
                m_fb_width = int(float(size.Width) * m_options.scale_factor);
                m_fb_height = int(float(size.Height) * m_options.scale_factor);
                createFrameBuffer(m_fb_width, m_fb_height);

                D3D11_TEXTURE2D_DESC desc{};
                surface->GetDesc(&desc);

                CopyParams params{};
                params.pixel_width = (float(size.Width) / float(desc.Width)) / m_fb_width;
                params.pixel_height = (float(size.Height) / float(desc.Height)) / m_fb_height;
                params.grayscale = m_options.grayscale ? 1 : 0;
                createConstantBuffer(params);
            }

            // dispatch copy
            {

                auto* cb = m_constants.get();
                auto* srv = m_srv_surface.get();
                auto* uav = m_uav_framebuffer.get();
                auto* smp = m_sampler.get();
                m_context->CSSetConstantBuffers(0, 1, &cb);
                m_context->CSSetShaderResources(0, 1, &srv);
                m_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
                m_context->CSSetSamplers(0, 1, &smp);
                m_context->CSSetShader(m_cs_copy.get(), nullptr, 0);

                m_context->Dispatch(m_fb_width, m_fb_height, 1);
            }

            if (m_options.cpu_readable) {
                // copy to cpu-readably buffer
                m_context->CopyResource(m_stagingbuffer.get(), m_framebuffer.get());

                m_handler(m_stagingbuffer.get());
            }
            else {
                m_handler(m_framebuffer.get());
            }

            //{
            //    D3D11_BOX box{};
            //    box.right = size.Width;
            //    box.bottom = size.Height;
            //    box.back = 1;
            //    m_context->CopySubresourceRegion(m_stagingbuffer.get(), 0, 0, 0, 0, surface.get(), 0, &box);
            //    m_handler(m_stagingbuffer.get());
            //}
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
