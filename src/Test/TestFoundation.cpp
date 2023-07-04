#include "pch.h"
#include "Test.h"
#include "Marionette.h"

testCase(String)
{
    std::string data = R"(Name:0.2 WithSpace: "hoge.png" Parenthesis : {"hage.png", 2})";

    int n = 0;
    mr::ScanKVP(data, [&](std::string k, std::string v) {
        if (n == 0) {
            testExpect(k == "Name" && mr::ToValue<float>(v) == 0.2f);
        }
        else if (n == 1) {
            testExpect(k == "WithSpace" && mr::ToValue<std::string>(v) == "hoge.png");
        }
        else if (n == 2) {
            testExpect(k == "Parenthesis" && v == "\"hage.png\", 2");
        }
        ++n;
        });
}


struct Hoge
{
    template<class U, class T2, std::enable_if_t<std::is_pointer_v<T2>, int> = 0>
    U read(T2& p)
    {
        U ret = (U&)*p;
        p += sizeof(U);
        return ret;
    }

    template<class U, class T2, std::enable_if_t<std::is_class_v<T2>, int> = 0>
    U read(T2& p)
    {
        U ret;
        char* dst = reinterpret_cast<char*>(&ret);
        for (size_t i = 0; i < sizeof(U); ++i)
        {
            *dst++ = *p++;
        }
        return ret;
    }
};

struct TestRef
{
    int value = 0;
};
bool operator<(const TestRef& a, const TestRef& b)
{
    return a.value < b.value;
}

testCase(Serialize)
{
    uint64_t data = 0x0102030405060708;
    char* srcp = (char*)&data;

    std::vector<char> datav;
    datav.assign(srcp, srcp + 8);
    auto srci = datav.begin();

    int dst[2]{};
    auto clear = [&]() {
        for (int i = 0; i < 2; ++i)
            dst[i] = 0;
    };

    {
        Hoge s;
        dst[0] = s.read<int>(srcp);
        dst[1] = s.read<int>(srcp);
    }

    clear();

    {
        Hoge s;
        dst[0] = s.read<int>(srci);
        dst[1] = s.read<int>(srci);
    }

    clear();


    std::string strv[2]{ "abc", "def" };
    std::array<std::string, 2> stra{ "ghi", "jkl" };

    auto f1 = std::async(std::launch::deferred, [stra = std::move(stra)]() {
        for (auto& s : stra) {
            printf("%s\n", s.c_str());
        }
    });
    f1.get();



    TestRef r1, r2;
    r2.value = 100;
    std::map<std::reference_wrapper<TestRef>, int> map;
    map[r1] = 100;
    map[r2] = 100;
}
