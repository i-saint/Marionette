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


class Resize : public IFilter
{
public:
    Resize();
    void setImage(Texture2DPtr v);
    void setResult(Texture2DPtr v);
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
    void setImage(Texture2DPtr v);
    void setResult(Texture2DPtr v);
    void setBlockSize(int v);

    void dispatch() override;
    void clear() override;

private:
    Texture2DPtr m_src;
    Texture2DPtr m_dst;

    CSContext m_ctx;
};


class TemplateMatch : public IFilter
{
public:
    TemplateMatch();
    void setImage(Texture2DPtr v);
    void setTemplate(Texture2DPtr v);

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
    struct Result
    {
        int2 pos_min;
        int2 pos_max;
        float val_min;
        float val_max;
        int pad[2];
    };
    mrCheck16(Result);

    ReduceMinMax();
    void setImage(Texture2DPtr v);

    void dispatch() override;
    void clear() override;

    Result getResult();

private:
    Texture2DPtr m_src;
    BufferPtr m_result;
    BufferPtr m_staging;
    std::future<Result> m_task;

    CSContext m_ctx1;
    CSContext m_ctx2;
};


class FilterManager
{
public:

private:
    Resize m_resize;
    Contour m_contour;
    TemplateMatch m_template_match;
    ReduceMinMax m_reduce_minmax;
};

} // namespace mr

