#pragma once
#include <type_traits>

namespace mr {

template<class T>
class releaser
{
public:
    static void addRef(T* v) { v->addRef(); }
    static void release(T* v) { v->release(); }
};

template<class T, class Releaser = releaser<T>>
class ref_ptr
{
public:
    ref_ptr() {}
    ref_ptr(T* p) { reset(p); }
    ref_ptr(T&& p) { swap(p); }
    ref_ptr(const ref_ptr& v) { reset(v.m_ptr); }
    ref_ptr& operator=(const ref_ptr& v) { reset(v.m_ptr); return *this; }

    template<class U>
    ref_ptr(const ref_ptr<U>& v)
    {
        static_assert(std::is_base_of_v<T, U>);
        reset(v.get());
    }

    template<class U>
    ref_ptr& operator=(const ref_ptr<U>& v)
    {
        static_assert(std::is_base_of_v<T, U>);
        reset(v.get());
        return *this;
    }

    ~ref_ptr() { reset(); }

    void reset(T* p = nullptr)
    {
        if (m_ptr)
            Releaser::release(m_ptr);
        m_ptr = p;
        if (m_ptr)
            Releaser::addRef(m_ptr);
    }

    void swap(ref_ptr& v)
    {
        std::swap(m_ptr, v->m_data);
    }

    T* release()
    {
        T* r = m_ptr;
        m_ptr = {};
        return r;
    }

    T* get() const { return m_ptr; }

    T& operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }
    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }
    operator T* () const { return m_ptr; }
    operator bool() const { return m_ptr; }
    bool operator==(const ref_ptr<T>& v) const { return m_ptr == v.m_ptr; }
    bool operator!=(const ref_ptr<T>& v) const { return m_ptr != v.m_ptr; }

private:
    T* m_ptr{};
};

template<class T, class... U>
inline ref_ptr<T> make_ref(U&&... v)
{
    return ref_ptr<T>(new T(std::forward<U>(v)...));
}


} // namespace mr
