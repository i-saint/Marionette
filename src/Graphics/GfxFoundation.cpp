#include "pch.h"
#include "GfxFoundation.h"
#include "Filter.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#pragma comment(lib, "d3d11.lib")

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
        AddInitializeHandler([]() { GfxGlobals::initializeInstance(); });
        AddFinalizeHandler([]() { GfxGlobals::finalizeInstance(); });
    }
} s_register_gfx;



static std::unique_ptr<GfxGlobals>& GetDeviceManagerPtr()
{
    static std::unique_ptr<GfxGlobals> s_inst;
    return s_inst;
}

bool GfxGlobals::initializeInstance()
{
    auto& inst = GetDeviceManagerPtr();
    if (!inst) {
        inst = std::make_unique<GfxGlobals>();
        return inst->initialize();
    }
    return false;
}

bool GfxGlobals::finalizeInstance()
{
    auto& inst = GetDeviceManagerPtr();
    if (inst) {
        inst = nullptr;
        return true;
    }
    return false;
}

GfxGlobals* GfxGlobals::get()
{
    return GetDeviceManagerPtr().get();
}

GfxGlobals::GfxGlobals()
{
}

GfxGlobals::~GfxGlobals()
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

bool GfxGlobals::initialize()
{
    // create device
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef mrDebug
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    com_ptr<ID3D11Device> device;
    ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, nullptr);
    if (!device) {
        return false;
    }

    device->QueryInterface(IID_PPV_ARGS(&m_device));
    if (!m_device) {
        return false;
    }

    // device context
    com_ptr<ID3D11DeviceContext> context;
    m_device->GetImmediateContext(context.put());
    if (!context) {
        return false;
    }
    context->QueryInterface(IID_PPV_ARGS(&m_context));
    if (!m_context) {
        return false;
    }

    // fence
    m_device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));

    // sampler
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        m_device->CreateSamplerState(&desc, m_sampler.put());
    }

    // shaders
    {
        m_cs_transform = std::make_shared<TransformCS>();
        m_cs_binarize = std::make_shared<BinarizeCS>();
        m_cs_contour = std::make_shared<ContourCS>();
        m_cs_template_match = std::make_shared<TemplateMatchCS>();
        m_cs_reduce_minmax = std::make_shared<ReduceMinMaxCS>();
    }

    return true;
}

bool GfxGlobals::valid() const
{
    return m_device != nullptr;
}

ID3D11Device5* GfxGlobals::getDevice()
{
    return m_device.get();
}

ID3D11DeviceContext4* GfxGlobals::getContext()
{
    return m_context.get();
}

uint64_t GfxGlobals::addFenceEvent()
{
    uint64_t fv = ++m_fence_value;
    m_context->Signal(m_fence.get(), fv);
    return fv;
}

bool GfxGlobals::waitFence(uint64_t v, uint32_t timeout_ms)
{
    if (SUCCEEDED(m_fence->SetEventOnCompletion(v, m_fence_event))) {
        if (::WaitForSingleObject(m_fence_event, timeout_ms) == WAIT_OBJECT_0) {
            return true;
        }
    }
    return false;
}

void GfxGlobals::flush()
{
    m_context->Flush();
}

bool GfxGlobals::sync(int timeout_ms)
{
    auto fv = addFenceEvent();
    flush();
    return waitFence(fv, timeout_ms);
}

ID3D11SamplerState* GfxGlobals::getDefaultSampler()
{
    return m_sampler.get();
}

void GfxGlobals::lock()
{
    m_mutex.lock();
}

void GfxGlobals::unlock()
{
    m_mutex.unlock();
}

#define DefCSGetter(Class, Member)\
    template<> Class* GfxGlobals::getCS<Class>() { return Member.get(); }

DefCSGetter(TransformCS, m_cs_transform);
DefCSGetter(BinarizeCS, m_cs_binarize);
DefCSGetter(ContourCS, m_cs_contour);
DefCSGetter(TemplateMatchCS, m_cs_template_match);
DefCSGetter(ReduceMinMaxCS, m_cs_reduce_minmax);

#undef DefCSGetter




BufferPtr Buffer::createConstant(uint32_t size, const void* data)
{
    auto ret = std::make_shared<Buffer>();

    ret->m_size = size;
    {
        D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_IMMUTABLE, 0, 0, 0, size };
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        D3D11_SUBRESOURCE_DATA sd{ data, 0, 0 };
        mrGfxDevice()->CreateBuffer(&desc, &sd, ret->m_buffer.put());
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
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

        D3D11_SUBRESOURCE_DATA sd{ data, 0, 0 };
        mrGfxDevice()->CreateBuffer(&desc, data ? &sd : nullptr, ret->m_buffer.put());
    }
    if (ret->m_buffer) {
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = size / stride;
            mrGfxDevice()->CreateShaderResourceView(ret->m_buffer.get(), &desc, ret->m_srv.put());
        }
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = size / stride;
            mrGfxDevice()->CreateUnorderedAccessView(ret->m_buffer.get(), &desc, ret->m_uav.put());
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

int Buffer::getSize() const
{
    return m_size;
}

int Buffer::getStride() const
{
    return m_stride;
}

void Buffer::download(int size)
{
    if (!m_staging) {
        D3D11_BUFFER_DESC desc{ (UINT)m_size, D3D11_USAGE_STAGING, 0, 0, 0, (UINT)m_stride };
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        mrGfxDevice()->CreateBuffer(&desc, nullptr, m_staging.put());
    }

    if (size != 0)
        DispatchCopy(m_staging.get(), m_buffer.get(), size);
    else
        DispatchCopy(m_staging.get(), m_buffer.get());
}

bool Buffer::map(const ReadCallback& callback)
{
    return MapRead(m_staging.get(), [&](const void* data) {
        callback(data);
        });
}

bool Buffer::read(const ReadCallback& callback, int size)
{
    download(size);
    return map(callback);
}


Texture2D* Texture2D::create_(uint32_t w, uint32_t h, TextureFormat format, const void* data, uint32_t pitch)
{
    if (w <= 0 || h <= 0)
        return nullptr;

    auto ret = std::make_unique<Texture2D>();
    ret->m_size = { (int)w, (int)h };
    ret->m_format = format;
    auto dxformat = GetDXFormat(format);
    {
        D3D11_TEXTURE2D_DESC desc{ w, h, 1, 1, dxformat, { 1, 0 }, D3D11_USAGE_DEFAULT, 0, 0, 0 };
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

        D3D11_SUBRESOURCE_DATA sd{ data, pitch, 0 };
        mrGfxDevice()->CreateTexture2D(&desc, data ? &sd : nullptr, ret->m_texture.put());
    }
    if (ret->m_texture) {
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = dxformat;
            desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipLevels = 1;
            mrGfxDevice()->CreateShaderResourceView(ret->m_texture.get(), &desc, ret->m_srv.put());
        }
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = dxformat;
            desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipSlice = 0;
            mrGfxDevice()->CreateUnorderedAccessView(ret->m_texture.get(), &desc, ret->m_uav.put());
        }
    }
    return ret->valid() ? ret.release() : nullptr;
}

Texture2D* Texture2D::create_(const char* path)
{
    Texture2D* ret{};

    int w, h, ch;
    byte* data = stbi_load(path, &w, &h, &ch, 0);
    if (data) {
        if (ch == 1) {
            ret = create_(w, h, TextureFormat::Ru8, data, w * 1);
        }
        else if (ch == 4) {
            ret = create_(w, h, TextureFormat::RGBAu8, data, w * 4);
        }
        else if (ch == 3) {
            std::vector<byte> tmp(w * h * 4);
            for (int i = 0; i < h; ++i) {
                auto s = data + (w * 3 * i);
                auto d = tmp.data() + (w * 4 * i);
                for (int j = 0; j < w; ++j) {
                    d[0] = s[0];
                    d[1] = s[1];
                    d[2] = s[2];
                    d[3] = 255;
                    s += 3;
                    d += 4;
                }
            }
            ret = create_(w, h, TextureFormat::RGBAu8, tmp.data(), w * 4);
        }

        stbi_image_free(data);
    }
    return ret;
}

Texture2DPtr Texture2D::wrap(com_ptr<ID3D11Texture2D>& v)
{
    auto ret = std::make_shared<Texture2D>();

    D3D11_TEXTURE2D_DESC desc{};
    v->GetDesc(&desc);

    ret->m_texture = v;
    ret->m_size = { (int)desc.Width, (int)desc.Height };
    ret->m_format = GetMRFormat(desc.Format);
    if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = desc.Format;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;
        mrGfxDevice()->CreateShaderResourceView(ret->m_texture.get(), &desc, ret->m_srv.put());
    }
    if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
        desc.Format = desc.Format;
        desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;
        mrGfxDevice()->CreateUnorderedAccessView(ret->m_texture.get(), &desc, ret->m_uav.put());
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

void Texture2D::release()
{
    delete this;
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

int2 Texture2D::getSize() const
{
    return m_size;
}

TextureFormat Texture2D::getFormat() const
{
    return m_format;
}

void Texture2D::download()
{
    if (!m_staging) {
        D3D11_TEXTURE2D_DESC desc{ (UINT)m_size.x, (UINT)m_size.y, 1, 1, GetDXFormat(m_format), { 1, 0 }, D3D11_USAGE_STAGING, 0, 0, 0 };
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        mrGfxDevice()->CreateTexture2D(&desc, nullptr, m_staging.put());
    }
    DispatchCopy(m_staging.get(), m_texture.get());
}

bool Texture2D::map(const ReadCallback& callback)
{
    return MapRead(m_staging.get(), [&](const void* data, int pitch) {
        callback(data, pitch);
        });
}

bool Texture2D::read(const ReadCallback& callback)
{
    download();
    return map(callback);
}

bool Texture2D::saveImpl(const std::string& path, int2 size, TextureFormat format, const void* data, int pitch)
{
    bool ret = false;
    if (format == TextureFormat::RGBAu8) {
        ret = stbi_write_png(path.c_str(), size.x, size.y, 4, data, pitch);
    }
    else if (format == TextureFormat::Ru8) {
        ret = stbi_write_png(path.c_str(), size.x, size.y, 1, data, pitch);
    }
    else if (format == TextureFormat::Rf32) {
        std::vector<byte> buf(size.x * size.y);
        for (int i = 0; i < size.y; ++i) {
            auto s = (const float*)((const byte*)data + (pitch * i));
            auto d = buf.data() + (size.x * i);
            for (int j = 0; j < size.x; ++j) {
                *d++ = byte(*s++ * 255.0f);
            }
        }
        ret = stbi_write_png(path.c_str(), size.x, size.y, 1, buf.data(), size.x);
    }
    else if (format == TextureFormat::Ri32) {
        std::vector<byte> buf(size.x * 32 * size.y);
        for (int i = 0; i < size.y; ++i) {
            auto s = (const uint32_t*)((const byte*)data + (pitch * i));
            auto d = buf.data() + (size.x * 32 * i);
            for (int j = 0; j < size.x; ++j) {
                uint32_t v = *s++;
                for (int k = 0; k < 32; ++k)
                    *d++ = (v & (1 << k)) ? 255 : 0;
            }
        }
        ret = stbi_write_png(path.c_str(), size.x * 32, size.y, 1, buf.data(), size.x * 32);
    }
    else {
        mrDbgPrint("Texture2D::save(): unknown format\n");
    }
    return ret;
}

bool Texture2D::save(const std::string& path)
{
    // copy data to temporary buffer to minimize Map() time
    std::vector<byte> buf;
    int pitch{};
    bool ret = read([&](const void* data, int pitch_) {
        pitch = pitch_;
        buf.resize(pitch * m_size.y);
        memcpy(buf.data(), data, buf.size());
        });

    // write to file
    if (ret)
        ret = saveImpl(path, m_size, m_format, buf.data(), pitch);
    return ret;
}

std::future<bool> Texture2D::saveAsync(const std::string& path)
{
    std::vector<byte> buf;
    int pitch{};
    bool ret = read([&](const void* data, int pitch_) {
        pitch = pitch_;
        buf.resize(pitch * m_size.y);
        memcpy(buf.data(), data, buf.size());
        });

    return std::async(std::launch::async, [path, size = m_size, format = m_format, pitch, buf = std::move(buf)]() {
        if (buf.empty())
            return false;
        return saveImpl(path, size, format, buf.data(), pitch);
        });
}



ComputeShader::~ComputeShader()
{

}

bool ComputeShader::initialize(const void* bin, size_t size)
{
    mrGfxDevice()->CreateComputeShader(bin, size, nullptr, m_shader.put());
    return m_shader != nullptr;
}

void ComputeShader::setCBuffer(BufferPtr v, int slot)
{
    if (slot >= m_cbuffers.size())
        m_cbuffers.resize(slot + 1);
    m_cbuffers[slot] = v->ptr();
}

void ComputeShader::setSRV(DeviceResourcePtr v, int slot)
{
    if (slot >= m_srvs.size())
        m_srvs.resize(slot + 1);
    m_srvs[slot] = v->srv();
}

void ComputeShader::setUAV(DeviceResourcePtr v, int slot)
{
    if (slot >= m_uavs.size())
        m_uavs.resize(slot + 1);
    m_uavs[slot] = v->uav();
}

void ComputeShader::setSampler(ID3D11SamplerState* v, int slot)
{
    if (slot >= m_samplers.size())
        m_samplers.resize(slot + 1);
    m_samplers[slot] = v;
}

std::vector<ID3D11Buffer*> ComputeShader::getConstants() { return m_cbuffers; }
std::vector<ID3D11ShaderResourceView*> ComputeShader::getSRVs() { return m_srvs; }
std::vector<ID3D11UnorderedAccessView*> ComputeShader::getUAVs() { return m_uavs; }
std::vector<ID3D11SamplerState*> ComputeShader::getSamplers() { return m_samplers; }


void ComputeShader::dispatch(int x, int y, int z)
{
    auto ctx = mrGfxContext();
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

    static void* nulls[32]{};
    if (!m_cbuffers.empty())
        ctx->CSSetConstantBuffers(0, m_cbuffers.size(), (ID3D11Buffer**)nulls);
    if (!m_srvs.empty())
        ctx->CSSetShaderResources(0, m_srvs.size(), (ID3D11ShaderResourceView**)nulls);
    if (!m_uavs.empty())
        ctx->CSSetUnorderedAccessViews(0, m_uavs.size(), (ID3D11UnorderedAccessView**)nulls, nullptr);
    if (!m_samplers.empty())
        ctx->CSSetSamplers(0, m_samplers.size(), (ID3D11SamplerState**)nulls);
}

void ComputeShader::clear()
{
    m_cbuffers.clear();
    m_srvs.clear();
    m_uavs.clear();
    m_samplers.clear();
}



TextureFormat GetMRFormat(DXGI_FORMAT f)
{
    switch (f) {
    case DXGI_FORMAT_R8_UNORM: return TextureFormat::Ru8;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return TextureFormat::RGBAu8;
    case DXGI_FORMAT_R32_FLOAT: return TextureFormat::Rf32;
    case DXGI_FORMAT_R32_SINT: return TextureFormat::Ri32;
    default: return TextureFormat::Unknown;
    }
}

DXGI_FORMAT GetDXFormat(TextureFormat f)
{
    switch (f) {
    case TextureFormat::Ru8: return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::RGBAu8: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::Rf32: return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::Ri32: return DXGI_FORMAT_R32_UINT;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

void DispatchCopy(ID3D11Resource* dst, ID3D11Resource* src)
{
    mrGfxContext()->CopyResource(dst, src);
}

void DispatchCopy(ID3D11Resource* dst, ID3D11Resource* src, int size, int src_offset, int dst_offset)
{
    D3D11_BOX box{};
    box.left = src_offset;
    box.right = size;
    box.bottom = 1;
    box.back = 1;
    mrGfxContext()->CopySubresourceRegion(dst, 0, dst_offset, 0, 0, src, 0, &box);
}

void DispatchCopy(ID3D11Resource* dst, ID3D11Resource* src, int2 size, int2 src_offset, int2 dst_offset)
{
    D3D11_BOX box{};
    box.left = src_offset.x;
    box.right = size.x;
    box.top = src_offset.y;
    box.bottom = size.y;
    box.back = 1;
    mrGfxContext()->CopySubresourceRegion(dst, 0, dst_offset.x, dst_offset.y, 0, src, 0, &box);
}

bool MapRead(ID3D11Buffer* src, const std::function<void(const void* data)>& callback)
{
    auto ctx = mrGfxContext();
    mrGfxFlush();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(src, 0, D3D11_MAP_READ, 0, &mapped))) {
        callback(mapped.pData);
        ctx->Unmap(src, 0);
        return true;
    }
    return false;
}

bool MapRead(ID3D11Texture2D* buf, const std::function<void(const void* data, int pitch)>& callback)
{
    auto ctx = mrGfxContext();
    mrGfxFlush();

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

mrAPI bool SaveAsPNG(const char* path, int w, int h, PixelFormat format, const void* data, int pitch, bool flip_y)
{
    if (pitch == 0) {
        switch (format) {
        case PixelFormat::Ru8: pitch = w * 1; break;
        case PixelFormat::BGRAu8: pitch = w * 4; break;
        case PixelFormat::RGBAu8: pitch = w * 4; break;
        default: break;
        }
    }

    if (format == PixelFormat::Ru8) {
        return stbi_write_png(path, w, h, 1, data, pitch);
    }
    else if (format == PixelFormat::BGRAu8) {
        std::vector<byte> buf(w * h * 4);
        int dst_pitch = w * 4;

        auto src = (const byte*)data;
        for (int i = 0; i < h; ++i) {
            auto s = src + (flip_y ? (pitch * (h - i - 1)) : (pitch * i));
            auto d = buf.data() + (dst_pitch * i);
            for (int j = 0; j < w; ++j) {
                d[0] = s[2];
                d[1] = s[1];
                d[2] = s[0];
                d[3] = s[3];
                s += 4;
                d += 4;
            }
        }
        return stbi_write_png(path, w, h, 4, buf.data(), dst_pitch);
    }
    else if (format == PixelFormat::RGBAu8) {
        return stbi_write_png(path, w, h, 4, data, pitch);
    }
    return false;
}

} // namespace mr
