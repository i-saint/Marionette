#pragma once

namespace mr {

// thin wrapper for Windows' event
class FenceEvent
{
public:
    FenceEvent();
    FenceEvent(const FenceEvent& v);
    FenceEvent& operator=(const FenceEvent& v);
    ~FenceEvent();
    operator HANDLE() const;

private:
    HANDLE m_handle = nullptr;
};


template<class Int> inline Int ceildiv(Int a, Int b) { return (a + (b - 1)) / b; }

} // namespace mr