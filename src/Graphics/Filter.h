#pragma once
#include "GfxFoundation.h"

namespace mr {

class TransformCS : public ICompute
{
public:
    TransformCS();
    void dispatch(ICSContext& ctx) override;
    TransformPtr createContext();

private:
    ComputeShader m_cs;
};

class Transform : public RefCount<ITransform>
{
public:
    Transform(TransformCS* v);
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
    ContourPtr createContext();

private:
    ComputeShader m_cs;
};

class Contour : public RefCount<IContour>
{
public:
    Contour(ContourCS* v);
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
    BinarizePtr createContext();

private:
    ComputeShader m_cs;
};

class Binarize : public RefCount<IBinarize>
{
public:
    Binarize(BinarizeCS* v);
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
    TemplateMatchPtr createContext();

private:
    ComputeShader m_cs_grayscale;
    ComputeShader m_cs_binary;
};

class TemplateMatch : public RefCount<ITemplateMatch>
{
public:
    TemplateMatch(TemplateMatchCS* v);
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
    ReduceTotalPtr createContext();

private:
    ComputeShader m_cs_pass1;
    ComputeShader m_cs_pass2;
};

class ReduceTotal : public RefCount<IReduceTotal>
{
public:
    ReduceTotal(ReduceTotalCS* v);
    void setSrc(ITexture2DPtr v) override;
    float getResult() override;
    void dispatch() override;

public:
    ReduceTotalCS* m_cs{};
    Texture2DPtr m_src;
    BufferPtr m_dst;
};


class ReduceCountBitsCS : public ICompute
{
public:
    ReduceCountBitsCS();
    void dispatch(ICSContext& ctx) override;
    ReduceCountBitsPtr createContext();

private:
    ComputeShader m_cs_pass1;
    ComputeShader m_cs_pass2;
};

class ReduceCountBits : public RefCount<IReduceCountBits>
{
public:
    ReduceCountBits(ReduceCountBitsCS* v);
    void setSrc(ITexture2DPtr v) override;
    uint32_t getResult() override;
    void dispatch() override;

public:
    ReduceCountBitsCS* m_cs{};
    Texture2DPtr m_src;
    BufferPtr m_dst;
};


class ReduceMinMaxCS : public ICompute
{
public:

    ReduceMinMaxCS();
    void dispatch(ICSContext& ctx) override;
    ReduceMinMaxPtr createContext();

private:
    ComputeShader m_cs_pass1;
    ComputeShader m_cs_pass2;
};

class ReduceMinMax : public RefCount<IReduceMinMax>
{
public:
    mrCheck16(Result);

    ReduceMinMax(ReduceMinMaxCS* v);
    void setSrc(ITexture2DPtr v) override;
    Result getResult() override;
    void dispatch() override;

public:
    ReduceMinMaxCS* m_cs{};
    Texture2DPtr m_src;
    BufferPtr m_dst;
};


} // namespace mr

