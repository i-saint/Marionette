#include "pch.h"
#include "Filter.h"

// shader binaries
#include "Copy.hlsl.h"
#include "Contour.hlsl.h"
#include "MatchGrayscale.hlsl.h"
#include "ReduceMinMax_Pass1.hlsl.h"
#include "ReduceMinMax_Pass2.hlsl.h"

#define PassBin(A) g_hlsl_Copy, std::size(g_hlsl_Copy)

namespace mr {

Resize::Resize(com_ptr<ID3D11Device>& device)
{
    m_ctx.initialize(device, PassBin(g_hlsl_Copy));
}


Contour::Contour(com_ptr<ID3D11Device>& device)
{
    m_ctx.initialize(device, PassBin(g_hlsl_Contour));
}


TemplateMatch::TemplateMatch(com_ptr<ID3D11Device>& device)
{
    m_ctx_grayscale.initialize(device, PassBin(g_hlsl_MatchGrayscale));
    m_ctx_binary.initialize(device, PassBin(g_hlsl_MatchBinary));
}


ReduceMinMax::ReduceMinMax(com_ptr<ID3D11Device>& device)
{
    m_ctx1.initialize(device, PassBin(g_hlsl_ReduceMinMax_Pass1));
    m_ctx2.initialize(device, PassBin(g_hlsl_ReduceMinMax_Pass2));
}

} // namespace mr
