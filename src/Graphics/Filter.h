#pragma once
#include "GfxFoundation.h"

namespace mr {

class IFilter
{
public:
    virtual ~IFilter();
    virtual void dispatch() = 0;
    virtual void clear() = 0;

protected:

};


class Transform : public IFilter
{
public:
    Transform();
    void setSrcImage(Texture2DPtr v);
    void setDstImage(Texture2DPtr v);
    void setCopyRegion(int2 pos, int2 size);
    void setFlipRB(bool v);
    void setGrayscale(bool v);

    void dispatch() override;
    void clear() override;

private:
    Texture2DPtr m_src;
    Texture2DPtr m_dst;
    BufferPtr m_const;

    int2 m_pos{};
    int2 m_size{};
    bool m_flip_rb = false;
    bool m_grayscale = false;
    bool m_dirty = true;

    CSContext m_ctx;
};


class Contour : public IFilter
{
public:
    Contour();
    void setSrcImage(Texture2DPtr v);
    void setDstImage(Texture2DPtr v);
    void setBlockSize(int v);

    void dispatch() override;
    void clear() override;

private:
    Texture2DPtr m_src;
    Texture2DPtr m_dst;
    BufferPtr m_const;

    int m_block_size = 5;
    bool m_dirty = true;

    CSContext m_ctx;
};


class Binalize : public IFilter
{
public:
    Binalize();
    void setSrcImage(Texture2DPtr v);
    void setDstImage(Texture2DPtr v);
    void setThreshold(int v);

    void dispatch() override;
    void clear() override;

private:
    Texture2DPtr m_src;
    Texture2DPtr m_dst;
    BufferPtr m_const;

    float m_threshold = 0.5f;
    bool m_dirty = true;

    CSContext m_ctx;
};


class TemplateMatch : public IFilter
{
public:
    TemplateMatch();
    void setSrcImage(Texture2DPtr v);
    void setDstImage(Texture2DPtr v);
    void setTemplateImage(Texture2DPtr v);

    void dispatch() override;
    void clear() override;

private:
    Texture2DPtr m_src;
    Texture2DPtr m_template;
    Texture2DPtr m_dst;

    CSContext m_ctx_grayscale;
    CSContext m_ctx_binary;
};


class ReduceMinMax : public IFilter
{
public:
    ReduceMinMax();
    void setSrcImage(Texture2DPtr v);

    void dispatch() override;
    void clear() override;

    std::future<ReduceMinmaxResult> getResult();

private:
    Texture2DPtr m_src;
    BufferPtr m_dst;
    BufferPtr m_staging;

    CSContext m_ctx1;
    CSContext m_ctx2;
};


} // namespace mr

