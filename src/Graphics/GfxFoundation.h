#pragma once

#include <winrt/Windows.Foundation.h>
#include <d3d11.h>
#include "Vector.h"
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

};
#define mrGetDevice() DeviceManager::get()->getDevice()
#define mrGetContext() DeviceManager::get()->getContext()


class DeviceObject
{
public:
    virtual ~DeviceObject() {};
    virtual bool valid() const = 0;
    virtual ID3D11Resource* ptr() = 0;
    virtual ID3D11ShaderResourceView* srv() = 0;
    virtual ID3D11UnorderedAccessView* uav() = 0;
};
mrDeclPtr(DeviceObject);

class Buffer : public DeviceObject
{
public:
    static std::shared_ptr<Buffer> createConstant(uint32_t size, const void* data);
    static std::shared_ptr<Buffer> createStructured(uint32_t size, uint32_t stride, const void* data = nullptr);
    static std::shared_ptr<Buffer> createStaging(uint32_t size, uint32_t stride = 0);

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
mrDeclPtr(Buffer);


class Texture2D : public DeviceObject
{
public:
    static std::shared_ptr<Texture2D> create(uint32_t w, uint32_t h, DXGI_FORMAT format, const void* data = nullptr, uint32_t stride = 0);
    static std::shared_ptr<Texture2D> createStaging(uint32_t w, uint32_t h, DXGI_FORMAT format);

    bool operator==(const Texture2D& v) const;
    bool operator!=(const Texture2D& v) const;
    bool valid() const override;
    ID3D11Texture2D* ptr() override;
    ID3D11ShaderResourceView* srv() override;
    ID3D11UnorderedAccessView* uav() override;

    int2 size() const;
    DXGI_FORMAT format() const;

private:
    int2 m_size{};
    DXGI_FORMAT m_format{};
    com_ptr<ID3D11Texture2D> m_texture;
    com_ptr<ID3D11ShaderResourceView> m_srv;
    com_ptr<ID3D11UnorderedAccessView> m_uav;
};
mrDeclPtr(Texture2D);


class CSContext
{
public:
    virtual ~CSContext();

    bool initialize(const void *bin, size_t size);

    void setCBuffer(BufferPtr v, int slot = 0);
    void setSRV(DeviceObjectPtr v, int slot = 0);
    void setUAV(DeviceObjectPtr v, int slot = 0);
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


void DispatchCopy(BufferPtr a, BufferPtr b, int size, int offset = 0);
void DispatchCopy(Texture2DPtr a, Texture2DPtr b, int2 size, int2 offset = int2::zero());
bool MapRead(BufferPtr v, const std::function<void(const void* data)>& callback);
bool MapRead(Texture2DPtr v, const std::function<void(const void* data, int pitch)>& callback);


} // namespace mr
