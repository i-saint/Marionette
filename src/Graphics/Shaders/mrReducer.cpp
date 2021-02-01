#include "pch.h"
#include "Graphics/mrShader.h"

// shader binaries
#include "ReduceTotal_FPass1.hlsl.h"
#include "ReduceTotal_FPass2.hlsl.h"
#include "ReduceTotal_IPass1.hlsl.h"
#include "ReduceTotal_IPass2.hlsl.h"

#include "ReduceCountBits_Pass1.hlsl.h"
#include "ReduceCountBits_Pass2.hlsl.h"

#include "ReduceMinMax_FPass1.hlsl.h"
#include "ReduceMinMax_FPass2.hlsl.h"
#include "ReduceMinMax_IPass1.hlsl.h"
#include "ReduceMinMax_IPass2.hlsl.h"

#define mrBytecode(A) A, std::size(A)

#define mrCheckDirty(...)\
    if (__VA_ARGS__) { return; }\
    m_dirty = true;

namespace mr {

template<class T>
class ReduceCommon : public RefCount<T>
{
public:
    void setSrc(ITexture2DPtr v) override;
    void setRegion(Rect v) override;
    int2 getSize() const override;
    Rect getRegion() const override;
    IBufferPtr getDst() const override;

    BufferPtr getParamsBuffer();
    BufferPtr createParamsBuffer();

public:
    Texture2DPtr m_src;
    BufferPtr m_dst;
    BufferPtr m_buf_params;

    bool m_dirty = true;
    Rect m_region{};
};

template<class T> void ReduceCommon<T>::setSrc(ITexture2DPtr v)
{
    mrCheckDirty(m_src.get() == v.get());
    m_src = cast(v);
}

template<class T> void ReduceCommon<T>::setRegion(Rect v)
{
    mrCheckDirty(m_region == v);
    m_region = v;
}

template<class T> int2 ReduceCommon<T>::getSize() const
{
    return m_region.size.x == 0 ? (m_src ? m_src->getSize() : int2::zero()) : m_region.size;
}

template<class T> Rect ReduceCommon<T>::getRegion() const
{
    return { m_region.pos, getSize() };
}

template<class T> IBufferPtr ReduceCommon<T>::getDst() const
{
    return m_dst;
}

template<class T> BufferPtr ReduceCommon<T>::getParamsBuffer()
{
    if (m_src && m_dirty) {
        m_buf_params = createParamsBuffer();
        m_dirty = false;
    }
    return m_buf_params;
}

template<class T> BufferPtr ReduceCommon<T>::createParamsBuffer()
{
    struct {
        int2 range;
        int2 tl;
        int2 br;
        int2 pad;
    } params{};
    params.range = getSize();
    params.tl = m_region.pos;
    params.br = params.tl + params.range;

    return Buffer::createConstant(params);
}


class ReduceTotal : public ReduceCommon<IReduceTotal>
{
public:
    ReduceTotal(ReduceTotalCS* v);
    Result getResult() override;
    void dispatch() override;

public:
    ReduceTotalCS* m_cs{};
};

ReduceTotal::ReduceTotal(ReduceTotalCS* v) : m_cs(v) {}

ReduceTotal::Result ReduceTotal::getResult()
{
    Result ret{};
    if (!m_dst)
        return ret;

    m_dst->map([&ret](const void* v) {
        ret = *(Result*)v;
        });
    return ret;
}

void ReduceTotal::dispatch()
{
    if (!m_src)
        return;

    size_t rsize = m_src->getSize().y * sizeof(float);
    if (!m_dst || m_dst->getSize() != rsize) {
        m_dst = Buffer::createStructured(rsize, sizeof(float));
    }

    m_cs->dispatch(*this);
    m_dst->download(sizeof(float));
}

ReduceTotalCS::ReduceTotalCS()
{
    m_cs_fpass1.initialize(mrBytecode(g_hlsl_ReduceTotal_FPass1));
    m_cs_fpass2.initialize(mrBytecode(g_hlsl_ReduceTotal_FPass2));
    m_cs_ipass1.initialize(mrBytecode(g_hlsl_ReduceTotal_IPass1));
    m_cs_ipass2.initialize(mrBytecode(g_hlsl_ReduceTotal_IPass2));
}

void ReduceTotalCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<ReduceTotal&>(ctx_);

    auto do_dispatch = [this, &ctx](auto& pass1, auto& pass2) {
        auto size = ctx.getSize();
        pass1.setCBuffer(ctx.getParamsBuffer());
        pass1.setSRV(ctx.m_src);
        pass1.setUAV(ctx.m_dst);
        pass1.dispatch(1, size.y);

        pass2.setCBuffer(ctx.getParamsBuffer());
        pass2.setSRV(ctx.m_src);
        pass2.setUAV(ctx.m_dst);
        pass2.dispatch(1, 1);
    };

    if (IsIntFormat(ctx.m_src->getFormat()))
        do_dispatch(m_cs_ipass1, m_cs_ipass2);
    else
        do_dispatch(m_cs_fpass1, m_cs_fpass2);
}

IReduceTotalPtr ReduceTotalCS::createContext()
{
    return make_ref<ReduceTotal>(this);
}



class ReduceCountBits : public ReduceCommon<IReduceCountBits>
{
public:
    ReduceCountBits(ReduceCountBitsCS* v);
    uint32_t getResult() override;
    void dispatch() override;

public:
    ReduceCountBitsCS* m_cs{};
};

ReduceCountBits::ReduceCountBits(ReduceCountBitsCS* v) : m_cs(v) {}

uint32_t ReduceCountBits::getResult()
{
    uint32_t ret{};
    if (!m_dst)
        return ret;

    m_dst->map([&ret](const void* v) {
        ret = *(uint32_t*)v;
        });
    return ret;
}

void ReduceCountBits::dispatch()
{
    if (!m_src)
        return;

    size_t rsize = m_src->getSize().y * sizeof(uint32_t);
    if (!m_dst || m_dst->getSize() != rsize) {
        m_dst = Buffer::createStructured(rsize, sizeof(uint32_t));
    }

    m_cs->dispatch(*this);
    m_dst->download(sizeof(uint32_t));
}

ReduceCountBitsCS::ReduceCountBitsCS()
{
    m_cs_pass1.initialize(mrBytecode(g_hlsl_ReduceCountBits_Pass1));
    m_cs_pass2.initialize(mrBytecode(g_hlsl_ReduceCountBits_Pass2));
}

void ReduceCountBitsCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<ReduceCountBits&>(ctx_);
    auto size = ctx.getSize();

    m_cs_pass1.setCBuffer(ctx.getParamsBuffer());
    m_cs_pass1.setSRV(ctx.m_src);
    m_cs_pass1.setUAV(ctx.m_dst);
    m_cs_pass1.dispatch(1, size.y);

    m_cs_pass2.setCBuffer(ctx.getParamsBuffer());
    m_cs_pass2.setSRV(ctx.m_src);
    m_cs_pass2.setUAV(ctx.m_dst);
    m_cs_pass2.dispatch(1, 1);
}

IReduceCountBitsPtr ReduceCountBitsCS::createContext()
{
    return make_ref<ReduceCountBits>(this);
}



class ReduceMinMax : public ReduceCommon<IReduceMinMax>
{
public:
    mrCheck16(Result);

    ReduceMinMax(ReduceMinMaxCS* v);
    Result getResult() override;
    void dispatch() override;

public:
    ReduceMinMaxCS* m_cs{};
};

ReduceMinMax::ReduceMinMax(ReduceMinMaxCS* v) : m_cs(v) {}

ReduceMinMax::Result ReduceMinMax::getResult()
{
    Result ret{};
    if (!m_dst)
        return ret;

    m_dst->map([&ret](const void* v) {
        ret = *(Result*)v;
        });
    return ret;
}

void ReduceMinMax::dispatch()
{
    if (!m_src)
        return;

    size_t rsize = m_src->getSize().y * sizeof(Result);
    if (!m_dst || m_dst->getSize() != rsize) {
        m_dst = Buffer::createStructured(rsize, sizeof(Result));
    }

    m_cs->dispatch(*this);
    m_dst->download(sizeof(Result));
}

ReduceMinMaxCS::ReduceMinMaxCS()
{
    m_cs_fpass1.initialize(mrBytecode(g_hlsl_ReduceMinMax_FPass1));
    m_cs_fpass2.initialize(mrBytecode(g_hlsl_ReduceMinMax_FPass2));
    m_cs_ipass1.initialize(mrBytecode(g_hlsl_ReduceMinMax_IPass1));
    m_cs_ipass2.initialize(mrBytecode(g_hlsl_ReduceMinMax_IPass2));
}

void ReduceMinMaxCS::dispatch(ICSContext& ctx_)
{
    auto& ctx = static_cast<ReduceMinMax&>(ctx_);

    auto do_dispatch = [this, &ctx](auto& pass1, auto& pass2) {
        auto size = ctx.getSize();
        pass1.setCBuffer(ctx.getParamsBuffer());
        pass1.setSRV(ctx.m_src);
        pass1.setUAV(ctx.m_dst);
        pass1.dispatch(1, size.y);

        pass2.setCBuffer(ctx.getParamsBuffer());
        pass2.setSRV(ctx.m_src);
        pass2.setUAV(ctx.m_dst);
        pass2.dispatch(1, 1);
    };

    if (IsIntFormat(ctx.m_src->getFormat()))
        do_dispatch(m_cs_ipass1, m_cs_ipass2);
    else
        do_dispatch(m_cs_fpass1, m_cs_fpass2);
}

IReduceMinMaxPtr ReduceMinMaxCS::createContext()
{
    return make_ref<ReduceMinMax>(this);
}

} // namespace mr
