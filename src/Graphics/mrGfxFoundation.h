#pragma once

#include <winrt/Windows.Foundation.h>
#include <d3d11.h>
#include "mrInternal.h"
using winrt::com_ptr;

#define mrCheck16(T) static_assert(sizeof(T) % 16 == 0)

namespace mr {

mrDeclPtr(DeviceResource);
mrDeclPtr(Buffer);
mrDeclPtr(Texture2D);

#define Body(Name) mrDeclPtr(Name##CS);
mrEachCS(Body)
#undef Body


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


class GfxGlobals
{
public:
    static GfxGlobals* get();

    bool valid() const;
    ID3D11Device5* getDevice();
    ID3D11DeviceContext4* getContext();

    uint64_t addFenceEvent();
    bool waitFence(uint64_t v, uint32_t timeout_ms = 1000);
    void flush();
    bool sync(int timeout_ms = 1000);

    ID3D11SamplerState* getPointSampler();
    ID3D11SamplerState* getLinearSampler();

    void lock();
    void unlock();

    template<class Body>
    inline void lock(const Body& body)
    {
        std::lock_guard lock(*this);
        body();
    }

    template<class CS>
    CS* getCS();

public:
    GfxGlobals();
    ~GfxGlobals();
    GfxGlobals(const GfxGlobals&) = delete;
    bool initialize();

private:
    com_ptr<ID3D11Device5> m_device;
    com_ptr<ID3D11DeviceContext4> m_context;

    com_ptr<ID3D11Fence> m_fence;
    FenceEvent m_fence_event;
    uint64_t m_fence_value = 0;

    com_ptr<ID3D11SamplerState> m_sampler_point;
    com_ptr<ID3D11SamplerState> m_sampler_linear;

    std::mutex m_mutex;

    // shaders
#define Body(Name) Name##CSPtr m_cs_##Name;
    mrEachCS(Body)
#undef Body
};
#define mrGfxGlobals() GfxGlobals::get()
#define mrGfxDevice() mrGfxGlobals()->getDevice()
#define mrGfxContext() mrGfxGlobals()->getContext()
#define mrGfxPointSampler() mrGfxGlobals()->getPointSampler()
#define mrGfxLinearSampler() mrGfxGlobals()->getLinearSampler()
#define mrGfxFlush(...) mrGfxGlobals()->flush()
#define mrGfxSync(...) mrGfxGlobals()->sync(__VA_ARGS__)
#define mrGfxLock(Body) mrGfxGlobals()->lock(Body)
#define mrGfxLockScope() std::lock_guard<GfxGlobals> _gfx_lock(*mrGfxGlobals())
#define mrGfxGetCS(Class) mrGfxGlobals()->getCS<Class>()

#define mrConvertile(T, I)\
    inline T* cast(I* v) { return static_cast<T*>(v); }\
    inline T& cast(I& v) { return static_cast<T&>(v); }





class DeviceResource
{
public:
    virtual ~DeviceResource() {};
    virtual bool valid() const = 0;
    virtual ID3D11Resource* ptr() = 0;
    virtual ID3D11ShaderResourceView* srv() = 0;
    virtual ID3D11UnorderedAccessView* uav() = 0;
};


class Buffer : public DeviceResource, public RefCount<IBuffer>
{
public:
    static BufferPtr createConstant(uint32_t size, const void* data);
    static BufferPtr createStructured(uint32_t size, uint32_t stride, const void* data = nullptr);

    template<class T>
    static inline BufferPtr createConstant(const T& v)
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

    int getSize() const override;
    int getStride() const override;

    void download(int size = 0) override;
    bool map(const ReadCallback& callback) override;
    bool read(const ReadCallback& callback, int size = 0) override; // download() & map()

    com_ptr<ID3D11Buffer>& get() { return m_buffer; }

private:
    int m_size{};
    int m_stride{};
    com_ptr<ID3D11Buffer> m_buffer;
    com_ptr<ID3D11Buffer> m_staging;
    com_ptr<ID3D11ShaderResourceView> m_srv;
    com_ptr<ID3D11UnorderedAccessView> m_uav;
};
mrConvertile(Buffer, IBuffer);


class Texture2D : public DeviceResource, public RefCount<ITexture2D>
{
public:
    static Texture2DPtr create(uint32_t w, uint32_t h, TextureFormat format, const void* data = nullptr, uint32_t pitch = 0);
    static Texture2DPtr create(const char* path);
    static Texture2DPtr wrap(com_ptr<ID3D11Texture2D>& v);

    bool operator==(const Texture2D& v) const;
    bool operator!=(const Texture2D& v) const;

    bool valid() const override;
    ID3D11Texture2D* ptr() override;
    ID3D11ShaderResourceView* srv() override;
    ID3D11UnorderedAccessView* uav() override;

    int2 getSize() const override;
    int2 getInternalSize() const;
    TextureFormat getFormat() const override;

    void download() override;
    bool map(const ReadCallback& callback) override;
    bool read(const ReadCallback& callback) override;

    static bool saveImpl(const std::string& path, int2 size, TextureFormat format, const void* data, int pitch);
    bool save(const std::string& path) override;
    std::future<bool> saveAsync(const std::string& path) override;

    com_ptr<ID3D11Texture2D>& get() { return m_texture; }

private:
    int2 m_size{};
    TextureFormat m_format{};
    com_ptr<ID3D11Texture2D> m_texture;
    com_ptr<ID3D11Texture2D> m_staging;
    com_ptr<ID3D11ShaderResourceView> m_srv;
    com_ptr<ID3D11UnorderedAccessView> m_uav;
};
mrConvertile(Texture2D, ITexture2D);


class ComputeShader
{
public:
    bool initialize(const void *bin, size_t size);

    void setCBuffer(Buffer* v, int slot = 0);
    void setSRV(DeviceResource* v, int slot = 0);
    void setUAV(DeviceResource* v, int slot = 0);
    void setSampler(ID3D11SamplerState* v, int slot = 0);

    std::vector<ID3D11Buffer*> getConstants();
    std::vector<ID3D11ShaderResourceView*> getSRVs();
    std::vector<ID3D11UnorderedAccessView*> getUAVs();
    std::vector<ID3D11SamplerState*> getSamplers();

    void dispatch(int x = 1, int y = 1, int z = 1);
    void clear();

private:
    com_ptr<ID3D11ComputeShader> m_shader;
    std::vector<ID3D11Buffer*> m_cbuffers;
    std::vector<ID3D11ShaderResourceView*> m_srvs;
    std::vector<ID3D11UnorderedAccessView*> m_uavs;
    std::vector<ID3D11SamplerState*> m_samplers;
};

class ICompute : public RefCount<IObject>
{
public:
    virtual ~ICompute();
    virtual void dispatch(ICSContext& ctx) = 0;
};


TextureFormat GetMRFormat(DXGI_FORMAT f);
DXGI_FORMAT GetDXFormat(TextureFormat f);
bool IsIntFormat(TextureFormat f);

void DispatchCopy(ID3D11Resource* dst, ID3D11Resource* src);
void DispatchCopy(ID3D11Resource* dst, ID3D11Resource* src, int size, int src_offset = 0, int dst_offset = 0);
void DispatchCopy(ID3D11Resource* dst, ID3D11Resource* src, int2 size, int2 src_offset = int2::zero(), int2 dst_offset = int2::zero());
bool MapRead(ID3D11Buffer* src, const std::function<void(const void* data)>& callback);
bool MapRead(ID3D11Texture2D* buf, const std::function<void(const void* data, int pitch)>& callback);


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

} // namespace mr
