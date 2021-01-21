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
    Resize(com_ptr<ID3D11Device>& device);
    void setImage(com_ptr<ID3D11ShaderResourceView>& v);
    void setResult(com_ptr<ID3D11UnorderedAccessView>& v);
    void setCopyRegion(int x, int y, int width, int height);
    void setScale(float v);
    void setFlipRB(bool v);
    void setGrayscale(bool v);

    void dispatch() override;
    void clear() override;

private:
    struct CopyParams
    {
        float2 pixel_size;
        float2 pixel_offset;
        float2 sample_step;
        int flip_rb;
        int grayscale;
    };
    static_assert(sizeof(CopyParams) % 16 == 0);

    CSContext m_ctx;
};


class Contour : public IFilter
{
public:
    Contour(com_ptr<ID3D11Device>& device);
    void setImage(com_ptr<ID3D11ShaderResourceView>& v);
    void setResult(com_ptr<ID3D11UnorderedAccessView>& v);
    void setBlockSize(int v);

    void dispatch() override;
    void clear() override;

private:
    CSContext m_ctx;
};


class TemplateMatch : public IFilter
{
public:
    TemplateMatch(com_ptr<ID3D11Device>& device);
    void setImage(com_ptr<ID3D11ShaderResourceView>& v);
    void setTemplate(com_ptr<ID3D11ShaderResourceView>& v);

    void dispatch() override;
    void clear() override;

private:
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

    ReduceMinMax(com_ptr<ID3D11Device>& device);
    void setImage(com_ptr<ID3D11ShaderResourceView>& v);
    void setResult(com_ptr<ID3D11UnorderedAccessView>& v);

    void dispatch() override;
    void clear() override;

    Result getResult();

private:
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

