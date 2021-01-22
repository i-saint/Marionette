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


static struct RegisterGfxInitializer
{
    RegisterGfxInitializer()
    {
        AddInitializeHandler([]() { DeviceManager::initialize(); });
        AddFinalizeHandler([]() { DeviceManager::finalize(); });
    }
} s_register_gfx;



static std::unique_ptr<DeviceManager>& GetDeviceManagerPtr()
{
    static std::unique_ptr<DeviceManager> s_inst;
    return s_inst;
}

bool DeviceManager::initialize()
{
    auto& inst = GetDeviceManagerPtr();
    if (!inst) {
        inst = std::make_unique<DeviceManager>();
        return true;
    }
    return false;
}

bool DeviceManager::finalize()
{
    auto& inst = GetDeviceManagerPtr();
    if (inst) {
        inst = nullptr;
        return true;
    }
    return false;
}

DeviceManager* DeviceManager::get()
{
    return GetDeviceManagerPtr().get();
}

DeviceManager::DeviceManager()
{
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef mrDebug
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    com_ptr<ID3D11Device> device;
    ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, nullptr);
    if (device)
        device->QueryInterface(IID_PPV_ARGS(&m_device));

    if (m_device) {
        com_ptr<ID3D11DeviceContext> context;
        m_device->GetImmediateContext(context.put());
        context->QueryInterface(IID_PPV_ARGS(&m_context));

        m_device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));

        {
            D3D11_SAMPLER_DESC desc{};
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            m_device->CreateSamplerState(&desc, m_sampler.put());
        }
    }
}

DeviceManager::~DeviceManager()
{
    m_sampler = nullptr;
    m_fence = nullptr;
    m_context = nullptr;

//#ifdef mrDebug
//    com_ptr<ID3D11Debug> debug;
//    m_device->QueryInterface(IID_PPV_ARGS(&debug));
//    if (debug) {
//        debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
//    }
//#endif // mrDebug
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

void DeviceManager::flush()
{
    m_context->Flush();
}

ID3D11SamplerState* DeviceManager::getDefaultSampler()
{
    return m_sampler.get();
}



BufferPtr Buffer::createConstant(uint32_t size, const void* data)
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

BufferPtr Buffer::createStructured(uint32_t size, uint32_t stride, const void* data)
{
    auto ret = std::make_shared<Buffer>();

    ret->m_size = size;
    ret->m_stride = stride;
    {
        D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_DEFAULT, 0, 0, 0, stride };
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

        D3D11_SUBRESOURCE_DATA sd{ data, 0, 0 };
        mrGetDevice()->CreateBuffer(&desc, data ? &sd : nullptr, ret->m_buffer.put());
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

std::shared_ptr<Buffer> Buffer::createStaging(uint32_t size, uint32_t stride)
{
    auto ret = std::make_shared<Buffer>();

    if (stride == 0)
        stride = size;
    ret->m_size = size;
    ret->m_stride = stride;
    {
        D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_STAGING, 0, 0, 0, stride };
        desc.BindFlags = D3D11_CPU_ACCESS_READ;

        mrGetDevice()->CreateBuffer(&desc, nullptr, ret->m_buffer.put());
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
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        mrGetDevice()->CreateTexture2D(&desc, nullptr, ret->m_texture.put());
    }
    return ret->valid() ? ret : nullptr;
}

std::shared_ptr<Texture2D> Texture2D::wrap(com_ptr<ID3D11Texture2D>& v)
{
    auto ret = std::make_shared<Texture2D>();

    D3D11_TEXTURE2D_DESC desc{};
    v->GetDesc(&desc);

    ret->m_texture = v;
    ret->m_size = { (int)desc.Width, (int)desc.Height };
    ret->m_format = desc.Format;
    if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = desc.Format;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        mrGetDevice()->CreateShaderResourceView(ret->m_texture.get(), &desc, ret->m_srv.put());
    }
    if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
        desc.Format = desc.Format;
        desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;
        mrGetDevice()->CreateUnorderedAccessView(ret->m_texture.get(), &desc, ret->m_uav.put());
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
    return m_size;
}

DXGI_FORMAT Texture2D::format() const
{
    return m_format;
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


void DispatchCopy(DeviceObjectPtr dst, DeviceObjectPtr src)
{
    mrGetContext()->CopyResource(dst->ptr(), src->ptr());
}

void DispatchCopy(BufferPtr dst, BufferPtr src, int size, int offset)
{
    D3D11_BOX box{};
    box.left = offset;
    box.right = size;
    box.bottom = 1;
    box.back = 1;
    mrGetContext()->CopySubresourceRegion(dst->ptr(), 0, 0, 0, 0, src->ptr(), 0, &box);
}

void DispatchCopy(Texture2DPtr dst, Texture2DPtr src, int2 size, int2 offset)
{
    D3D11_BOX box{};
    box.left = offset.x;
    box.right = size.x;
    box.top = offset.y;
    box.bottom = size.y;
    box.back = 1;
    mrGetContext()->CopySubresourceRegion(dst->ptr(), 0, 0, 0, 0, src->ptr(), 0, &box);
}

bool MapRead(BufferPtr src, const std::function<void (const void*)>& callback)
{
    auto ctx = mrGetContext();
    auto buf = src->ptr();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(buf, 0, D3D11_MAP_READ, 0, &mapped))) {
        callback(mapped.pData);
        ctx->Unmap(buf, 0);
        return true;
    }
    return false;
}

bool MapRead(Texture2DPtr src, const std::function<void(const void* data, int pitch)>& callback)
{
    auto ctx = mrGetContext();
    auto buf = src->ptr();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(buf, 0, D3D11_MAP_READ, 0, &mapped))) {
        D3D11_TEXTURE2D_DESC desc{};
        buf->GetDesc(&desc);

        callback(mapped.pData, mapped.RowPitch);

        ctx->Unmap(buf, 0);
        return true;
    }
    return false;
}


} // namespace mr
