#include "pch.h"
#include "GfxFoundation.h"

namespace mr {


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


CSContext::~CSContext()
{

}

bool CSContext::initialize(com_ptr<ID3D11Device>& device, const void* bin, size_t size)
{
    m_device->CreateComputeShader(bin, size, nullptr, m_shader.put());
    return m_shader != nullptr;
}

void CSContext::setConstant(com_ptr<ID3D11Buffer>& v, int slot)
{
    if (slot >= m_cbuffers.size())
        m_cbuffers.resize(slot + 1);
    m_cbuffers[slot] = v.get();
}

void CSContext::setSRV(com_ptr<ID3D11ShaderResourceView>& v, int slot)
{
    if (slot >= m_srvs.size())
        m_srvs.resize(slot + 1);
    m_srvs[slot] = v.get();
}

void CSContext::setUAV(com_ptr<ID3D11UnorderedAccessView>& v, int slot)
{
    if (slot >= m_uavs.size())
        m_uavs.resize(slot + 1);
    m_uavs[slot] = v.get();
}

void CSContext::setSampler(com_ptr<ID3D11SamplerState>& v, int slot)
{
    if (slot >= m_samplers.size())
        m_samplers.resize(slot + 1);
    m_samplers[slot] = v.get();
}

void CSContext::dispatch(int x, int y, int z)
{
    if (!m_cbuffers.empty())
        m_context->CSSetConstantBuffers(0, m_cbuffers.size(), m_cbuffers.data());
    if (!m_srvs.empty())
        m_context->CSSetShaderResources(0, m_srvs.size(), m_srvs.data());
    if (!m_uavs.empty())
        m_context->CSSetUnorderedAccessViews(0, m_uavs.size(), m_uavs.data(), nullptr);
    if (!m_samplers.empty())
        m_context->CSSetSamplers(0, m_samplers.size(), m_samplers.data());
    m_context->CSSetShader(m_shader.get(), nullptr, 0);
    m_context->Dispatch(x, y, z);
}

void CSContext::clear()
{
    m_cbuffers.clear();
    m_srvs.clear();
    m_uavs.clear();
    m_samplers.clear();
}

} // namespace mr
