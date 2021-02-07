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
