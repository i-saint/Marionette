#pragma once

#include <winrt/Windows.Foundation.h>
#include <d3d11.h>
#include "Vector.h"
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


class CSContext
{
public:
    virtual ~CSContext();

    bool initialize(com_ptr<ID3D11Device>& device, const void *bin, size_t size);

    void setConstant(com_ptr<ID3D11Buffer>& v, int slot = 0);
    void setSRV(com_ptr<ID3D11ShaderResourceView>& v, int slot = 0);
    void setUAV(com_ptr<ID3D11UnorderedAccessView>& v, int slot = 0);
    void setSampler(com_ptr<ID3D11SamplerState>& v, int slot = 0);

    void dispatch(int x, int y, int z = 1);
    void clear();

private:
    com_ptr<ID3D11Device> m_device;
    com_ptr<ID3D11DeviceContext> m_context;

    com_ptr<ID3D11ComputeShader> m_shader;
    std::vector<ID3D11Buffer*> m_cbuffers;
    std::vector<ID3D11ShaderResourceView*> m_srvs;
    std::vector<ID3D11UnorderedAccessView*> m_uavs;
    std::vector<ID3D11SamplerState*> m_samplers;
};


class DeviceManager
{
public:


private:
    com_ptr<ID3D11Device> m_device;
    com_ptr<ID3D11DeviceContext> m_context;
};

} // namespace mr
