#pragma once
#include "GfxFoundation.h"

namespace mr {

class TransformCS : public ICompute
{
public:
    TransformCS();
    void dispatch(ICSContext& ctx) override;
    TransformCtxPtr createContext();

private:
    ComputeShader m_cs;
};

class TransformCtx : public RefCount<ITransform>
{
public:
    TransformCtx(TransformCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setRect(int2 o, int2 s) override;
    void setScale(float v) override;
    void setGrayscale(bool v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    TransformCS* m_cs{};
    Texture2DPtr m_src;
    Texture2DPtr m_dst;
    BufferPtr m_const;

    int2 m_offset = int2::zero();
    int2 m_size = int2::zero();
    float m_scale = 1.0f;
    bool m_grayscale = false;
    bool m_dirty = true;
};


class ContourCS : public ICompute
{
public:
    ContourCS();
    void dispatch(ICSContext& ctx) override;
    ContourCtxPtr createContext();

private:
    ComputeShader m_cs;
};

class ContourCtx : public RefCount<IContour>
{
public:
    ContourCtx(ContourCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setBlockSize(int v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    ContourCS* m_cs{};
    Texture2DPtr m_dst;
    Texture2DPtr m_src;
    BufferPtr m_const;

    int m_block_size = 5;
    bool m_dirty = true;
};


class BinarizeCS : public ICompute
{
public:
    BinarizeCS();
    void dispatch(ICSContext& ctx) override;
    BinarizeCtxPtr createContext();

private:
    ComputeShader m_cs;
};

class BinarizeCtx : public RefCount<IBinarize>
{
public:
    BinarizeCtx(BinarizeCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setThreshold(float v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    BinarizeCS* m_cs{};
    Texture2DPtr m_dst;
    Texture2DPtr m_src;
    BufferPtr m_const;

    float m_threshold = 0.5f;
    bool m_dirty = true;
};


class TemplateMatchCS : public ICompute
{
public:
    TemplateMatchCS();
    void dispatch(ICSContext& ctx) override;
    TemplateMatchCtxPtr createContext();

private:
    ComputeShader m_cs_grayscale;
    ComputeShader m_cs_binary;
};

class TemplateMatchCtx : public RefCount<ITemplateMatch>
{
public:
    TemplateMatchCtx(TemplateMatchCS* v);
    void setSrc(ITexture2DPtr v) override;
    void setDst(ITexture2DPtr v) override;
    void setTemplate(ITexture2DPtr v) override;
    ITexture2DPtr getDst() override;
    void dispatch() override;

public:
    TemplateMatchCS* m_cs{};
    Texture2DPtr m_dst;
    Texture2DPtr m_src;
    Texture2DPtr m_template;
};



class ReduceTotalCS : public ICompute
{
public:
    ReduceTotalCS();
    void dispatch(ICSContext& ctx) override;
    ReduceTotalCtxPtr createContext();

private:
    ComputeShader m_pass1;
    ComputeShader m_pass2;
};

class ReduceCountBitsCS : public ICompute
{
public:
    ReduceCountBitsCS();
    void dispatch(ICSContext& ctx) override;
    ReduceCountBitsCtxPtr createContext();

private:
    ComputeShader m_pass1;
    ComputeShader m_pass2;
};


class ReduceMinMaxCS : public ICompute
{
public:

    ReduceMinMaxCS();
    void dispatch(ICSContext& ctx) override;
    ReduceMinMaxCtxPtr createContext();

private:
    ComputeShader m_cs_pass1;
    ComputeShader m_cs_pass2;
};

class ReduceMinMaxCtx : public RefCount<IReduceMinMax>
{
public:
    mrCheck16(Result);

    ReduceMinMaxCtx(ReduceMinMaxCS* v);
    void setSrc(ITexture2DPtr v) override;
    Result getResult() override;
    void dispatch() override;

public:
    ReduceMinMaxCS* m_cs{};
    Texture2DPtr m_src;
    BufferPtr m_dst;
};


} // namespace mr

