#pragma once

#include <winrt/Windows.Foundation.h>
#include <d3d11.h>
#include "Vector.h"
#include "Internal.h"
#include "MouseReplayer.h"
using winrt::com_ptr;

#define mrCheck16(T) static_assert(sizeof(T) % 16 == 0)

namespace mr {

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


class DeviceManager
{
public:
    static bool initialize();
    static bool finalize();
    static DeviceManager* get();

    bool valid() const;
    ID3D11Device5* getDevice();
    ID3D11DeviceContext4* getContext();

    uint64_t addFenceEvent();
    bool waitFence(uint64_t v, uint32_t timeout_ms = 1000);
    void flush();

    ID3D11SamplerState* getDefaultSampler();

public:
    DeviceManager();
    ~DeviceManager();
    DeviceManager(const DeviceManager&) = delete;

private:
    com_ptr<ID3D11Device5> m_device;
    com_ptr<ID3D11DeviceContext4> m_context;

    com_ptr<ID3D11Fence> m_fence;
    FenceEvent m_fence_event;
    uint64_t m_fence_value = 0;

    com_ptr<ID3D11SamplerState> m_sampler;

};
#define mrGetDevice() DeviceManager::get()->getDevice()
#define mrGetContext() DeviceManager::get()->getContext()
#define mrGetDefaultSampler() DeviceManager::get()->getDefaultSampler()


class DeviceResource;
class Buffer;
class Texture2D;
mrDeclPtr(DeviceResource);
mrDeclPtr(Buffer);
mrDeclPtr(Texture2D);


class DeviceResource
{
public:
    virtual ~DeviceResource() {};
    virtual bool valid() const = 0;
    virtual ID3D11Resource* ptr() = 0;
    virtual ID3D11ShaderResourceView* srv() = 0;
    virtual ID3D11UnorderedAccessView* uav() = 0;
};


class Buffer : public DeviceResource
{
public:
    static BufferPtr createConstant(uint32_t size, const void* data);
    static BufferPtr createStructured(uint32_t size, uint32_t stride, const void* data = nullptr);
    static BufferPtr createStaging(uint32_t size, uint32_t stride = 0);

    template<class T>
    static inline std::shared_ptr<Buffer> createConstant(const T& v)
    {
        mrCheck16(T);
        return createConstant(sizeof(v), &v);
    }

    bool operator==(const Buffer& v) const;
    bool operator!=(const Buffer& v) const;
    bool valid() const override;
    ID3D11Buffer* ptr() override;
    ID3D11ShaderResourceView* srv() override;
    ID3D11UnorderedAccessView* uav() override;

    size_t size() const;
    size_t stride() const;

private:
    size_t m_size{};
    size_t m_stride{};
    com_ptr<ID3D11Buffer> m_buffer;
    com_ptr<ID3D11ShaderResourceView> m_srv;
    com_ptr<ID3D11UnorderedAccessView> m_uav;
};


class Texture2D : public DeviceResource, public ITexture2D
{
public:
    static Texture2DPtr create(uint32_t w, uint32_t h, TextureFormat format, const void* data = nullptr, uint32_t stride = 0);
    static Texture2DPtr wrap(com_ptr<ID3D11Texture2D>& v);

    bool operator==(const Texture2D& v) const;
    bool operator!=(const Texture2D& v) const;

    void release() override;
    bool valid() const override;
    ID3D11Texture2D* ptr() override;
    ID3D11ShaderResourceView* srv() override;
    ID3D11UnorderedAccessView* uav() override;

    int2 getSize() const override;
    TextureFormat getFormat() const override;

    void readImpl();
    bool read(const ReadCallback& cb) override;
    std::future<bool> readAsync(const ReadCallback& callback) override;

private:
    int2 m_size{};
    TextureFormat m_format{};
    com_ptr<ID3D11Texture2D> m_texture;
    com_ptr<ID3D11Texture2D> m_staging;
    com_ptr<ID3D11ShaderResourceView> m_srv;
    com_ptr<ID3D11UnorderedAccessView> m_uav;
};


class CSContext
{
public:
    virtual ~CSContext();

    bool initialize(const void *bin, size_t size);

    void setCBuffer(BufferPtr v, int slot = 0);
    void setSRV(DeviceResourcePtr v, int slot = 0);
    void setUAV(DeviceResourcePtr v, int slot = 0);
    void setSampler(ID3D11SamplerState* v, int slot = 0);

    std::vector<ID3D11Buffer*> getConstants();
    std::vector<ID3D11ShaderResourceView*> getSRVs();
    std::vector<ID3D11UnorderedAccessView*> getUAVs();
    std::vector<ID3D11SamplerState*> getSamplers();

    void dispatch(int x, int y = 1, int z = 1);
    void clear();

private:
    com_ptr<ID3D11ComputeShader> m_shader;
    std::vector<ID3D11Buffer*> m_cbuffers;
    std::vector<ID3D11ShaderResourceView*> m_srvs;
    std::vector<ID3D11UnorderedAccessView*> m_uavs;
    std::vector<ID3D11SamplerState*> m_samplers;
};

TextureFormat GetMRFormat(DXGI_FORMAT f);
DXGI_FORMAT GetDXFormat(TextureFormat f);

void DispatchCopy(ID3D11Resource* dst, ID3D11Resource* src);
void DispatchCopy(DeviceResourcePtr dst, DeviceResourcePtr src);
void DispatchCopy(BufferPtr dst, BufferPtr src, int size, int offset = 0);
void DispatchCopy(Texture2DPtr dst, Texture2DPtr src, int2 size, int2 offset = int2::zero());
bool MapRead(BufferPtr src, const std::function<void(const void* data)>& callback);
bool MapRead(ID3D11Texture2D* buf, const std::function<void(const void* data, int pitch)>& callback);
bool MapRead(Texture2DPtr src, const std::function<void(const void* data, int pitch)>& callback);
void FlushCommands();

template<class To, class From>
inline com_ptr<To> As(From* ptr)
{
    com_ptr<To> ret;
    ptr->QueryInterface(guid_of<To>(), ret.put_void());
    return ret;
}

template<class To, class From>
inline com_ptr<To> As(com_ptr<From>& ptr)
{
    return As<To>(ptr.get());
}


#define mrWithDesktopDuplicationAPI
//#define mrWithWindowsGraphicsCapture

#ifdef mrWithDesktopDuplicationAPI

class IDesktopDuplication
{
public:
    using Callback = std::function<void(ID3D11Texture2D*, int w, int h)>;

    virtual ~IDesktopDuplication() {};
    virtual void release() = 0;
    virtual bool start(HMONITOR hmon = nullptr) = 0;
    virtual void stop() = 0;
    virtual bool getFrame(int timeout_ms, const Callback& calback) = 0;
};
mrDeclPtr(IDesktopDuplication);

IDesktopDuplication* CreateDesktopDuplication();
mrDefShared(CreateDesktopDuplication);

#endif // mrWithDesktopDuplicationAPI


#ifdef mrWithWindowsGraphicsCapture

class IGraphicsCapture
{
public:
    using Callback = std::function<void(ID3D11Texture2D*, int w, int h)>;

    virtual ~IGraphicsCapture() {};
    virtual void release() = 0;
    virtual bool start(HWND hwnd, const Callback& handler) = 0;
    virtual bool start(HMONITOR hmon, const Callback& handler) = 0;
    virtual void stop() = 0;
};
mrDeclPtr(IGraphicsCapture);

bool IsGraphicsCaptureSupported();
IGraphicsCapture* CreateGraphicsCapture();
mrDefShared(CreateGraphicsCapture);

#endif // mrWithWindowsGraphicsCapture

} // namespace mr
