#pragma once

namespace mr {


template<class Int> inline Int ceildiv(Int a, Int b) { return (a + (b - 1)) / b; }

} // namespace mr
