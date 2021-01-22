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



static std::unique_ptr<DeviceManager> g_device_manager;

bool DeviceManager::initialize()
{
    if (!g_device_manager) {
        g_device_manager = std::make_unique<DeviceManager>();
        return true;
    }
    return false;
}

bool DeviceManager::finalize()
{
    if (g_device_manager) {
        g_device_manager = nullptr;
        return true;
    }
    return false;
}

DeviceManager* DeviceManager::get()
{
    return g_device_manager.get();
}

DeviceManager::DeviceManager()
{
    {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef mrDebug
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        com_ptr<ID3D11Device> device;
        ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, nullptr);
        device->QueryInterface(IID_PPV_ARGS(&m_device));

        m_device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
    }
    {
        com_ptr<ID3D11DeviceContext> context;
        m_device->GetImmediateContext(context.put());
        context->QueryInterface(IID_PPV_ARGS(&m_context));
    }
}

DeviceManager::~DeviceManager()
{
}

bool DeviceManager::valid() const
{
    return m_device != nullptr;
}

ID3D11Device5* DeviceManager::getDevice()
{
    return m_device.get();
}

ID3D11DeviceContext4* DeviceManager::getContext()
{
    return m_context.get();
}

uint64_t DeviceManager::addFenceEvent()
{
    uint64_t fv = ++m_fence_value;
    m_context->Signal(m_fence.get(), fv);
    return fv;
}

bool DeviceManager::waitFence(uint64_t v, uint32_t timeout_ms)
{
    if (SUCCEEDED(m_fence->SetEventOnCompletion(v, m_fence_event))) {
        if (::WaitForSingleObject(m_fence_event, timeout_ms) == WAIT_OBJECT_0) {
            return true;
        }
    }
    return false;
}



BufferPtr Buffer::createConstant(const void* data, uint32_t size)
{
    auto ret = std::make_shared<Buffer>();

    ret->m_size = size;
    {
        D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_IMMUTABLE, 0, 0, 0, size };
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        D3D11_SUBRESOURCE_DATA sd{ data, 0, 0 };
        mrGetDevice()->CreateBuffer(&desc, &sd, ret->m_buffer.put());
    }
    return ret->valid() ? ret : nullptr;
}

BufferPtr Buffer::createStructured(const void* data, uint32_t size, uint32_t stride)
{
    auto ret = std::make_shared<Buffer>();

    ret->m_size = size;
    ret->m_stride = stride;
    {
        D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_DEFAULT, 0, 0, 0, stride };
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

        D3D11_SUBRESOURCE_DATA sd{ data, 0, 0 };
        mrGetDevice()->CreateBuffer(&desc, &sd, ret->m_buffer.put());
    }
    if (ret->m_buffer) {
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = size / stride;
            mrGetDevice()->CreateShaderResourceView(ret->m_buffer.get(), &desc, ret->m_srv.put());
        }
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = size / stride;
            mrGetDevice()->CreateUnorderedAccessView(ret->m_buffer.get(), &desc, ret->m_uav.put());
        }
    }
    return ret->valid() ? ret : nullptr;
}

bool Buffer::operator==(const Buffer& v) const
{
    return v.m_buffer == m_buffer;
}

bool Buffer::operator!=(const Buffer& v) const
{
    return v.m_buffer != m_buffer;
}

bool Buffer::valid() const
{
    return m_buffer != nullptr;
}

ID3D11Buffer* Buffer::ptr()
{
    return m_buffer.get();
}

ID3D11ShaderResourceView* Buffer::srv()
{
    return m_srv.get();
}

ID3D11UnorderedAccessView* Buffer::uav()
{
    return m_uav.get();
}

size_t Buffer::size() const
{
    return m_size;
}

size_t Buffer::stride() const
{
    return m_stride;
}


Texture2DPtr Texture2D::create(uint32_t w, uint32_t h, DXGI_FORMAT format, const void* data, uint32_t stride)
{
    auto ret = std::make_shared<Texture2D>();
    ret->m_size = { (int)w, (int)h };
    ret->m_format = format;
    {
        D3D11_TEXTURE2D_DESC desc{ w, h, 1, 1, format, { 1, 0 }, D3D11_USAGE_DEFAULT, 0, 0, 0 };
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

        D3D11_SUBRESOURCE_DATA sd{ data, stride, 0 };
        mrGetDevice()->CreateTexture2D(&desc, data ? &sd : nullptr, ret->m_texture.put());
    }
    if (ret->m_texture) {
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = format;
            desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipLevels = 1;
            mrGetDevice()->CreateShaderResourceView(ret->m_texture.get(), &desc, ret->m_srv.put());
        }
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = format;
            desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipSlice = 0;
            mrGetDevice()->CreateUnorderedAccessView(ret->m_texture.get(), &desc, ret->m_uav.put());
        }
    }
    return ret->valid() ? ret : nullptr;
}

Texture2DPtr Texture2D::createStaging(uint32_t w, uint32_t h, DXGI_FORMAT format)
{
    auto ret = std::make_shared<Texture2D>();
    ret->m_size = { (int)w, (int)h };
    ret->m_format = format;
    {
        D3D11_TEXTURE2D_DESC desc{ w, h, 1, 1, format, { 1, 0 }, D3D11_USAGE_STAGING, 0, 0, 0 };
        desc.BindFlags = D3D11_CPU_ACCESS_READ;
        mrGetDevice()->CreateTexture2D(&desc, nullptr, ret->m_texture.put());
    }
    return ret->valid() ? ret : nullptr;
}

bool Texture2D::operator==(const Texture2D& v) const
{
    return v.m_texture == m_texture;
}

bool Texture2D::operator!=(const Texture2D& v) const
{
    return v.m_texture != m_texture;
}

bool Texture2D::valid() const
{
    return m_texture != nullptr;
}

ID3D11Texture2D* Texture2D::ptr()
{
    return m_texture.get();
}

ID3D11ShaderResourceView* Texture2D::srv()
{
    return m_srv.get();
}

ID3D11UnorderedAccessView* Texture2D::uav()
{
    return m_uav.get();
}

int2 Texture2D::size() const
{
    return int2();
}

DXGI_FORMAT Texture2D::format() const
{
    return DXGI_FORMAT();
}



CSContext::~CSContext()
{

}

bool CSContext::initialize(const void* bin, size_t size)
{
    mrGetDevice()->CreateComputeShader(bin, size, nullptr, m_shader.put());
    return m_shader != nullptr;
}

void CSContext::setCBuffer(BufferPtr v, int slot)
{
    if (slot >= m_cbuffers.size())
        m_cbuffers.resize(slot + 1);
    m_cbuffers[slot] = v->ptr();
}

void CSContext::setSRV(DeviceObjectPtr v, int slot)
{
    if (slot >= m_srvs.size())
        m_srvs.resize(slot + 1);
    m_srvs[slot] = v->srv();
}

void CSContext::setUAV(DeviceObjectPtr v, int slot)
{
    if (slot >= m_uavs.size())
        m_uavs.resize(slot + 1);
    m_uavs[slot] = v->uav();
}

void CSContext::setSampler(ID3D11SamplerState* v, int slot)
{
    if (slot >= m_samplers.size())
        m_samplers.resize(slot + 1);
    m_samplers[slot] = v;
}

std::vector<ID3D11Buffer*> CSContext::getConstants() { return m_cbuffers; }
std::vector<ID3D11ShaderResourceView*> CSContext::getSRVs() { return m_srvs; }
std::vector<ID3D11UnorderedAccessView*> CSContext::getUAVs() { return m_uavs; }
std::vector<ID3D11SamplerState*> CSContext::getSamplers() { return m_samplers; }


void CSContext::dispatch(int x, int y, int z)
{
    auto ctx = mrGetContext();
    if (!m_cbuffers.empty())
        ctx->CSSetConstantBuffers(0, m_cbuffers.size(), m_cbuffers.data());
    if (!m_srvs.empty())
        ctx->CSSetShaderResources(0, m_srvs.size(), m_srvs.data());
    if (!m_uavs.empty())
        ctx->CSSetUnorderedAccessViews(0, m_uavs.size(), m_uavs.data(), nullptr);
    if (!m_samplers.empty())
        ctx->CSSetSamplers(0, m_samplers.size(), m_samplers.data());
    ctx->CSSetShader(m_shader.get(), nullptr, 0);
    ctx->Dispatch(x, y, z);
}

void CSContext::clear()
{
    m_cbuffers.clear();
    m_srvs.clear();
    m_uavs.clear();
    m_samplers.clear();
}



ID3D11Resource* GetResource(ID3D11View* view)
{
    return nullptr;
}

int2 GetSize(ID3D11Texture2D* tex)
{
    D3D11_TEXTURE2D_DESC desc{};
    tex->GetDesc(&desc);
    return { (int)desc.Width, (int)desc.Height };
}

com_ptr<ID3D11Buffer> CreateConstantBuffer(const void* data, size_t size_)
{
    auto size = (UINT)size_;
    D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, size };
    D3D11_SUBRESOURCE_DATA sd{ data, size, size };
    com_ptr<ID3D11Buffer> ret;
    mrGetDevice()->CreateBuffer(&desc, &sd, ret.put());
    return ret;
}

} // namespace mr
