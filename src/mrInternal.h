#pragma once
#include "Marionette.h"

namespace mr {

#define mrEnableIf(...) std::enable_if_t<__VA_ARGS__, bool> = true

template<class T, mrEnableIf(std::is_enum<T>::value)>
inline void set_flag(uint32_t& dst, T flag, bool v)
{
    if (v)
        (uint32_t&)dst |= (uint32_t)flag;
    else
        (uint32_t&)dst &= ~(uint32_t)flag;
}

template<class T, mrEnableIf(std::is_enum<T>::value)>
inline bool get_flag(uint32_t src, T flag)
{
    return (src & (uint32_t)flag) != 0;
}


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
class RefCount : public T
{
public:
    // forbid copy
    RefCount(const RefCount& v) = delete;
    RefCount& operator=(const RefCount& v) = delete;

    RefCount() {}

    int addRef() override
    {
        return ++m_ref;
    }

    int release() override
    {
        int ret = --m_ref;
        if (ret == 0)
            onRefCountZero();
        return ret;
    }

    int getRef() const override
    {
        return m_ref;
    }

    virtual void onRefCountZero()
    {
        delete this;
    }

protected:
    std::atomic_int m_ref{ 0 };
};

void AddInitializeHandler(const std::function<void()>& v);
void AddFinalizeHandler(const std::function<void()>& v);

} // namespace mr
