#pragma once

#ifdef _WIN32
    #define testExport extern "C" __declspec(dllexport)
#else
    #define testExport extern "C"
#endif

#define testPrint(...) ::test::PrintImpl(__VA_ARGS__)

#define testRegisterTestEntry(Name)\
    struct Register##Name {\
        Register##Name() { ::test::RegisterTestEntryImpl(#Name, Name); }\
    } g_Register##Name;

#define TestCase(Name) testExport void Name(); testRegisterTestEntry(Name); testExport void Name()
#define Expect(Body) if(!(Body)) { Print("%s(%d): failed - " #Body "\n", __FILE__, __LINE__); }


namespace test {

using nanosec = uint64_t;
nanosec Now();
inline float NS2MS(nanosec ns) { return float(double(ns) / 1000000.0); }

void RegisterTestEntryImpl(const char *name, const std::function<void()>& body);
void PrintImpl(const char *format, ...);
template<class T> bool GetArg(const char* name, T& dst);

template<class Body>
inline void TestScope(const char *name, const Body& body, int num_try = 1)
{
    auto begin = Now();
    for (int i = 0; i < num_try; ++i)
        body();
    auto end = Now();

    float elapsed = NS2MS(end - begin);
    testPrint("    %s: %.2fms", name, elapsed / num_try);
    if (num_try > 1) {
        testPrint(" (%.2fms in total)", elapsed);
    }
    testPrint("\n");
}

} // namespace test
