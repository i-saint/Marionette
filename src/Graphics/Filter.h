#pragma once
#include "GfxFoundation.h"

namespace mr {

class IFilter
{
public:
    virtual ~IFilter();

protected:

};


class Resize : public IFilter
{
public:
    Resize(com_ptr<ID3D11Device>& device);
    void setScale(float v);
    void setFlipRB(bool v);
    void setGrayscale(bool v);

private:
    CSContext m_ctx;
};


class Contour : public IFilter
{
public:
    Contour(com_ptr<ID3D11Device>& device);
    void setBlockSize(int v);

private:
    CSContext m_ctx;
};


class TemplateMatch : public IFilter
{
public:
    TemplateMatch(com_ptr<ID3D11Device>& device);

private:
    CSContext m_ctx_grayscale;
    CSContext m_ctx_binary;
};


class ReduceMinMax : public IFilter
{
public:
    struct Result
    {
        int pos_min_x;
        int pos_min_y;
        int pos_max_x;
        int pos_max_y;
        float val_min;
        float vmax;
        int pad[2];
    };
    mrCheck16(Result);


    ReduceMinMax(com_ptr<ID3D11Device>& device);

private:
    CSContext m_ctx1;
    CSContext m_ctx2;
};

} // namespace mr

