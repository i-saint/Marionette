#pragma once
#include "mrFoundation.h"
#include "mrInput.h"
#include "mrGfx.h"

namespace mr {

mrAPI void Initialize();
mrAPI void Finalize();

class InitializeScope
{
public:
    InitializeScope() { ::mr::Initialize(); }
    ~InitializeScope() { ::mr::Finalize(); }
};

} // namespace mr
