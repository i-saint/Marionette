#pragma once
#include "Foundation/mrVector.h"
#include "Foundation/mrRefPtr.h"

#define mrAPI extern "C" __declspec(dllexport)

#define mrDeclPtr(T)\
    class T;\
    using T##Ptr = ref_ptr<T>;

#define mrDefShared(F)\
    inline auto F##()\
    {\
        using T = std::remove_pointer_t<decltype(F##_())>;\
        return ref_ptr<T>(F##_());\
    }


#ifdef mrDebug
    #define mrEnableProfile
    #define mrDbgPrint(...) ::mr::Print(__VA_ARGS__)
#else
    //#define mrEnableProfile
    #define mrDbgPrint(...)
#endif

#ifdef mrEnableProfile
    #define mrProfile(...) ::mr::ProfileTimer _dbg_pftimer(__VA_ARGS__)
#else
    #define mrProfile(...)
#endif

namespace mr {

using millisec = uint64_t;
using nanosec = uint64_t;

std::string Format(const char* format, ...);
void Print(const char* fmt, ...);
void Print(const wchar_t* fmt, ...);
millisec NowMS();
nanosec NowNS();
void SleepMS(millisec v);
std::string GetCurrentModuleDirectory();

void Split(const std::string& str, const std::string& separator, const std::function<void(std::string sub)>& body);
void Scan(const std::string& str, const std::regex& exp, const std::function<void(std::string sub)>& body);
std::string Replace(const std::string& str, const std::string& before, const std::string& after);

template<class T> inline std::span<T> MakeSpan(T& v) { return { &v, 1 }; }
template<class T> inline std::span<T> MakeSpan(std::vector<T>& v) { return { v.data(), v.size() }; }
template<class T> inline std::span<T> MakeSpan(T* v, size_t n) { return { v, n }; }

class IObject
{
public:
    virtual int addRef() = 0;
    virtual int release() = 0;
    virtual int getRef() const = 0;

protected:
    virtual ~IObject() {}
};

} // namespace mr
