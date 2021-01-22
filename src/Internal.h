#pragma once

namespace mr {

void AddInitializeHandler(const std::function<void()>& v);
void AddFinalizeHandler(const std::function<void()>& v);


template<class Int> inline Int ceildiv(Int a, Int b) { return (a + (b - 1)) / b; }

} // namespace mr
