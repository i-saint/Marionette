#include "pch.h"
#include "Internal.h"
#include "ScreenCapture.h"
#include "Filter.h"


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

class GraphicsCapture : public IScreenCapture
{
public:
    GraphicsCapture();
    ~GraphicsCapture() override;

    void release() override;
    bool valid() const override;
    FrameInfo getFrame() override;

    template<class CreateCaptureItem>
    bool startImpl(const CreateCaptureItem& body);

    bool startCapture(HWND hwnd) override;
    bool startCapture(HMONITOR hmon) override;
    void stopCapture() override;

    void setOnFrameArrived(const Callback& cb) override;

    // called from capture thread
    void onFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

private:
    Callback m_callback;

    FrameInfo m_frame_info;
    std::mutex m_mutex;

    Texture2DPtr m_frame_buffer;
    TransformCtxPtr m_transform;

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
}

GraphicsCapture::~GraphicsCapture()
{
    stopCapture();
}

bool GraphicsCapture::valid() const
{
    return m_capture_session != nullptr;
}

GraphicsCapture::FrameInfo GraphicsCapture::getFrame()
{
    FrameInfo ret;
    {
        std::unique_lock l(m_mutex);
        ret = m_frame_info;
    }
    return ret;
}

void GraphicsCapture::release()
{
    delete this;
}

template<class CreateCaptureItem>
bool GraphicsCapture::startImpl(const CreateCaptureItem& cci)
{
    stopCapture();

    mrProfile("GraphicsCapture::start()");
    try {
        if (!m_transform)
            m_transform = mrGfxGetCS(TransformCS)->createContext();

        if (!m_device_rt) {
            auto dxgi = As<IDXGIDevice>(mrGfxDevice());
            com_ptr<::IInspectable> device_rt;
            check_hresult(::CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), device_rt.put()));
            m_device_rt = device_rt.as<IDirect3DDevice>();
        }

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

void GraphicsCapture::setOnFrameArrived(const Callback& cb)
{
    std::unique_lock l(m_mutex);
    m_callback = cb;
}

void GraphicsCapture::onFrameArrived(Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args)
{
    try {
        auto time = NowNS();
        auto frame = sender.TryGetNextFrame();
        auto size = frame.ContentSize();
        auto surface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

        auto src = Texture2D::wrap(surface);
        if (!m_frame_buffer) {
            m_frame_buffer = Texture2D::create(size.Width, size.Height, TextureFormat::RGBAu8);
        }

        m_transform->setSrc(src);
        m_transform->setDst(m_frame_buffer);
        m_transform->setRect({ 0, 0 }, { size.Width, size.Height });
        mrGfxLock([this]() {
            m_transform->dispatch();
            });

        FrameInfo tmp{ m_frame_buffer, time };
        {
            std::unique_lock l(m_mutex);
            m_frame_info = tmp;

            if (m_callback)
                m_callback(m_frame_info);
        }

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
    return new GraphicsCapture();
}

} // namespace mr
#endif // mrWithWindowsGraphicsCapture
