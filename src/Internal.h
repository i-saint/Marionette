#pragma once
#include "MouseReplayer.h"

namespace mr {

class Timer
{
public:
    Timer();
    void reset();
    float elapsed() const; // in sec

private:
    nanosec m_begin = 0;
};

class ProfileTimer : public Timer
{
public:
    ProfileTimer(const char* mes, ...);
    ~ProfileTimer();

private:
    std::string m_message;
};


template<class T>
class ImplRelease : public T
{
public:
    void release() override
    {
        delete this;
    }
};

void AddInitializeHandler(const std::function<void()>& v);
void AddFinalizeHandler(const std::function<void()>& v);


template<class Int> inline Int ceildiv(Int a, Int b) { return (a + (b - 1)) / b; }

} // namespace mr
