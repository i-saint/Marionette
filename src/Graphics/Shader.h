#pragma once
#include "GfxFoundation.h"

namespace mr {

class TransformCS : public ICompute
{
public:
    TransformCS();
    void dispatch(ICSContext& ctx) override;
    ITransformPtr createContext();

private:
    ComputeShader m_cs;
};


class NormalizeCS : public ICompute
{
public:
    NormalizeCS();
    void dispatch(ICSContext& ctx) override;
    INormalizePtr createContext();

private:
    ComputeShader m_cs_f;
    ComputeShader m_cs_i;
};


class BinarizeCS : public ICompute
{
public:
    BinarizeCS();
    void dispatch(ICSContext& ctx) override;
    IBinarizePtr createContext();

private:
    ComputeShader m_cs;
};


class ContourCS : public ICompute
{
public:
    ContourCS();
    void dispatch(ICSContext& ctx) override;
    IContourPtr createContext();

private:
    ComputeShader m_cs;
};


class ExpandCS : public ICompute
{
public:
    ExpandCS();
    void dispatch(ICSContext& ctx) override;
    IExpandPtr createContext();

private:
    ComputeShader m_cs_grayscale;
    ComputeShader m_cs_binary;
};


class TemplateMatchCS : public ICompute
{
public:
    TemplateMatchCS();
    void dispatch(ICSContext& ctx) override;
    ITemplateMatchPtr createContext();

private:
    ComputeShader m_cs_grayscale;
    ComputeShader m_cs_binary;
};


class ShapeCS : public ICompute
{
public:
    ShapeCS();
    void dispatch(ICSContext& ctx) override;
    IShapePtr createContext();

private:
    ComputeShader m_cs;
};



class ReduceTotalCS : public ICompute
{
public:
    ReduceTotalCS();
    void dispatch(ICSContext& ctx) override;
    IReduceTotalPtr createContext();

private:
    ComputeShader m_cs_fpass1;
    ComputeShader m_cs_fpass2;
    ComputeShader m_cs_ipass1;
    ComputeShader m_cs_ipass2;
};


class ReduceCountBitsCS : public ICompute
{
public:
    ReduceCountBitsCS();
    void dispatch(ICSContext& ctx) override;
    IReduceCountBitsPtr createContext();

private:
    ComputeShader m_cs_pass1;
    ComputeShader m_cs_pass2;
};


class ReduceMinMaxCS : public ICompute
{
public:
    ReduceMinMaxCS();
    void dispatch(ICSContext& ctx) override;
    IReduceMinMaxPtr createContext();

private:
    ComputeShader m_cs_fpass1;
    ComputeShader m_cs_fpass2;
    ComputeShader m_cs_ipass1;
    ComputeShader m_cs_ipass2;
};

} // namespace mr

