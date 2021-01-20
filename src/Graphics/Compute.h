#pragma once

#include <winrt/Windows.Foundation.h>
#include <d3d11.h>
using winrt::com_ptr;

namespace mr {

class CSContext
{
public:

private:
    com_ptr<ID3D11Device> m_device;
    com_ptr<ID3D11DeviceContext> m_context;

    com_ptr<ID3D11ComputeShader> m_cs_copy;
};

} // namespace mr
