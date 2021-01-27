#pragma once
#include "Marionette.h"

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
