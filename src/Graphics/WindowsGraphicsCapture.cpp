#include "pch.h"
#include "Internal.h"
#include "GfxFoundation.h"
#include "MouseReplayer.h"
#include "Filter.h"


#ifdef mrWithGraphicsCapture
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.Capture.Interop.h>

// shader binaries
#include "Copy.hlsl.h"

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

// ScreenCapture with Windows Graphics Capture

class ScreenCaptureWGC : public IScreenCapture
{
public:
    ScreenCaptureWGC();
    ~ScreenCaptureWGC() override;

    bool valid() const;
    void release() override;
    void setOptions(const Options& opt) override;
    bool start(HWND hwnd, const CaptureHandler& handler) override;
    bool start(HMONITOR hmon, const CaptureHandler& handler) override;
    void stop() override;

    bool getPixels(const PixelHandler& handler) override;

    template<class CreateCaptureItem>
    bool startImpl(const CreateCaptureItem& body);

    void onFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

private:
    Options m_options;
    Resize m_resize;

    Texture2DPtr m_surface;
    Texture2DPtr m_frame_buffer;
    Texture2DPtr m_staging_buffer;

    IDirect3DDevice m_device_rt{ nullptr };
    Direct3D11CaptureFramePool m_frame_pool{ nullptr };
    GraphicsCaptureItem m_capture_item{ nullptr };
    GraphicsCaptureSession m_capture_session{ nullptr };
    Direct3D11CaptureFramePool::FrameArrived_revoker m_frame_arrived;

    CaptureHandler m_handler;
};


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

ScreenCaptureWGC::ScreenCaptureWGC()
{
    InitializeGraphicsCapture();
}

ScreenCaptureWGC::~ScreenCaptureWGC()
{
    stop();
}

bool ScreenCaptureWGC::valid() const
{
    return m_capture_session != nullptr;
}

void ScreenCaptureWGC::release()
{
    delete this;
}

void ScreenCaptureWGC::setOptions(const Options& opt)
{
    m_options = opt;
}

template<class CreateCaptureItem>
bool ScreenCaptureWGC::startImpl(const CreateCaptureItem& cci)
{
    mrProfile("GraphicsCapture::start()");
    try {
        auto device = mrGetDevice();
        {
            auto dxgi = As<IDXGIDevice>(device);
            com_ptr<::IInspectable> device_rt;
            check_hresult(::CreateDirect3D11DeviceFromDXGIDevice(dxgi, device_rt.put()));
            m_device_rt = device_rt.as<IDirect3DDevice>();
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
                m_frame_arrived = m_frame_pool.FrameArrived(auto_revoke, { this, &ScreenCaptureWGC::onFrameArrived });
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

bool ScreenCaptureWGC::start(HWND hwnd, const CaptureHandler& handler)
{
    stop();

    m_handler = handler;
    startImpl([this, hwnd](auto& interop) {
        check_hresult(interop->CreateForWindow(hwnd, guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), put_abi(m_capture_item)));
        });
    return valid();
}

bool ScreenCaptureWGC::start(HMONITOR hmon, const CaptureHandler& handler)
{
    stop();

    m_handler = handler;
    startImpl([this, hmon](auto& interop) {
        check_hresult(interop->CreateForMonitor(hmon, guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), put_abi(m_capture_item)));
        });
    return valid();
}

void ScreenCaptureWGC::stop()
{
    m_frame_arrived.revoke();
    m_capture_session = nullptr;
    if (m_frame_pool) {
        m_frame_pool.Close();
        m_frame_pool = nullptr;
    }
}

bool ScreenCaptureWGC::getPixels(const PixelHandler& handler)
{
    if (!m_staging_buffer) {
        mrDbgPrint("GraphicsCapture::getPixels() require create_backbuffer and cpu_read option\n");
        return false;
    }

    {
        mrProfile("GraphicsCapture: copy texture (wait)");
        // wait for completion
        uint64_t fv = DeviceManager::get()->addFenceEvent();
        DeviceManager::get()->flush();
        DeviceManager::get()->waitFence(fv);
    }

    bool ret = false;
    {
        mrProfile("GraphicsCapture: map texture");

        // map & unmap
        ret = MapRead(m_staging_buffer, [&](const void* data, int pitch) {
            auto size = m_staging_buffer->size();
            handler(data, size.x, size.y, pitch);
            });
    }
    return ret;
}

void ScreenCaptureWGC::onFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args)
{
    try {
        auto frame = sender.TryGetNextFrame();
        auto size = frame.ContentSize();
        auto surface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

        if (!m_surface || m_surface->ptr() != surface.get()) {
            m_surface = Texture2D::wrap(surface);
        }

        // create staging texture if needed
        if (!m_frame_buffer) {
            int width = int(float(size.Width) * m_options.scale_factor);
            int height = int(float(size.Height) * m_options.scale_factor);
            auto format = m_options.grayscale ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
            m_frame_buffer = Texture2D::create(width, height, format);
            m_staging_buffer = Texture2D::createStaging(width, height, format);
        }

        // dispatch copy
        m_resize.setSrcImage(m_surface);
        m_resize.setDstImage(m_frame_buffer);
        m_resize.setCopyRegion({ 0, 0 }, { size.Width, size.Height });
        m_resize.setGrayscale(m_options.grayscale);
        m_resize.dispatch();

        DispatchCopy(m_staging_buffer, m_frame_buffer);

        m_handler(m_staging_buffer->ptr());

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

mrAPI IScreenCapture* CreateScreenCapture()
{
    if (!IsGraphicsCaptureSupported())
        return nullptr;
    return new ScreenCaptureWGC();
}

} // namespace mr
#endif // mrWithGraphicsCapture
