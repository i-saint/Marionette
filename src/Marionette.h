#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <future>
#include <thread>
#include <mutex>
#include <span>

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
