#include "pch.h"
#include "mrInternal.h"

namespace mr {

std::string OpRecord::toText() const
{
    switch (type)
    {
    case OpType::KeyDown:
        return Format("%lld: KeyDown %d", time, data.key.code);

    case OpType::KeyUp:
        return Format("%lld: KeyUp %d", time, data.key.code);

    case OpType::MouseDown:
        return Format("%lld: MouseDown %d", time, data.mouse.button);

    case OpType::MouseUp:
        return Format("%lld: MouseUp %d", time, data.mouse.button);

    case OpType::MouseMoveAbs:
        return Format("%lld: MouseMoveAbs %d %d", time, data.mouse.pos.x, data.mouse.pos.y);

    case OpType::MouseMoveRel:
        return Format("%lld: MouseMoveRel %d %d", time, data.mouse.pos.x, data.mouse.pos.y);

    case OpType::SaveMousePos:
        return Format("%lld: SaveMousePos %d", time, exdata.save_slot);

    case OpType::LoadMousePos:
        return Format("%lld: LoadMousePos %d", time, exdata.save_slot);

    case OpType::MatchParams:
    {
        std::string ret;
        auto& p = exdata.match_params;
        ret += "MatchParams";
        ret += Format(" Scale=%.2f", p.scale);
        ret += Format(" CareDisplayScale=%d", (int)p.care_display_scale);
        ret += Format(" ColorRange=%.2f,%.2f", p.color_range.x, p.color_range.y);
        ret += Format(" ContourRadius=%.2f", p.contour_radius);
        ret += Format(" ExpandRadius=%.2f", p.expand_radius);
        ret += Format(" BinarizeThreshold=%.2f", p.binarize_threshold);
        return ret;
    }

    case OpType::MouseMoveMatch:
    case OpType::WaitUntilMatch:
    {
        std::string ret;
        ret += Format("%lld: ", time);
        if (type == OpType::MouseMoveMatch)
            ret += "MouseMoveMatch";
        else if (type == OpType::WaitUntilMatch)
            ret += "WaitUntilMatch";

        for (auto& id : exdata.templates)
            ret += Format(" \"%s\"", id.path.c_str());
        return ret;
    }

    case OpType::Wait:
        return Format("%lld: Wait %d", time, exdata.wait_time);

    default:
        return "";
    }
}

bool OpRecord::fromText(const std::string& v)
{
    type = OpType::Unknown;
    const char* src = v.c_str();

    auto skip = [&src]() {
        while (*src != '\0') {
            if (src[0] == ' ' && src[1] != ' ') {
                ++src;
                break;
            }
            ++src;
        }
    };

    if (sscanf(src, "TimeShift %d", &exdata.time_shift) == 1)
        type = OpType::TimeShift;
    else if (sscanf(src, "%lld: KeyDown %d", &time, &data.key.code) == 2)
        type = OpType::KeyDown;
    else if (sscanf(src, "%lld: KeyUp %d", &time, &data.key.code) == 2)
        type = OpType::KeyUp;
    else if (sscanf(src, "%lld: MouseDown %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseDown;
    else if (sscanf(src, "%lld: MouseUp %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseUp;
    else if (sscanf(src, "%lld: MouseMoveAbs %d %d", &time, &data.mouse.pos.x, &data.mouse.pos.y) == 3)
        type = OpType::MouseMoveAbs;
    else if (sscanf(src, "%lld: MouseMoveRel %d %d", &time, &data.mouse.pos.x, &data.mouse.pos.y) == 3)
        type = OpType::MouseMoveRel;
    else if (sscanf(src, "%lld: SaveMousePos %d", &time, &exdata.save_slot) == 2)
        type = OpType::SaveMousePos;
    else if (sscanf(src, "%lld: LoadMousePos %d", &time, &exdata.save_slot) == 2)
        type = OpType::LoadMousePos;
    else if (std::strstr(src, "MatchParams ")) {
        type = OpType::MatchParams;

        auto& p = exdata.match_params;
        float fv;
        float2 f2v;
        int iv;
        skip();
        while (*src != '\0') {
            if (sscanf(src, "Scale=%f", &fv) == 1) p.scale = fv;
            else if (sscanf(src, "CareDisplayScale=%d", &iv) == 1) p.care_display_scale = iv != 0;
            else if (sscanf(src, "ColorRange=%f,%f", &f2v.x, &f2v.y) == 2) p.color_range = f2v;
            else if (sscanf(src, "ContourRadius=%f", &fv) == 1) p.contour_radius = fv;
            else if (sscanf(src, "ExpandRadius=%f", &fv) == 1) p.expand_radius = fv;
            else if (sscanf(src, "BinarizeThreshold=%f", &fv) == 1) p.binarize_threshold = fv;
            skip();
        }
    }
    else if ((std::strstr(src, "MouseMoveMatch") || std::strstr(src, "WaitUntilMatch")) && sscanf(src, "%lld: ", &time) == 1) {
        if (std::strstr(src, "MouseMoveMatch"))
            type = OpType::MouseMoveMatch;
        else if (std::strstr(src, "WaitUntilMatch"))
            type = OpType::WaitUntilMatch;

        Scan(src, std::regex("\"([^\"]+)\""), [this](std::string path) {
            exdata.templates.push_back({ 0, path });
            });
    }
    else if (sscanf(src, "%lld: Wait %d", &time, &exdata.wait_time) == 2) {
        type = OpType::Wait;
    }
    return type != OpType::Unknown;
}

std::map<Key, std::string> LoadKeymap(const char* path, const std::function<void(Key key, std::string path)>& body)
{
    std::map<Key, std::string> ret;

    std::ifstream ifs(path, std::ios::in);
    if (!ifs)
        return ret;

    static std::map<std::string, int> keymap
    {
        {"back", VK_BACK},
        {"tab", VK_TAB},
        {"clear", VK_CLEAR},
        {"enter", VK_RETURN},
        {"pause", VK_PAUSE},
        {"escape", VK_ESCAPE},
        {"space", VK_SPACE},
        {"f1", VK_F1},
        {"f2", VK_F2},
        {"f3", VK_F3},
        {"f4", VK_F4},
        {"f5", VK_F5},
        {"f6", VK_F6},
        {"f7", VK_F7},
        {"f8", VK_F8},
        {"f9", VK_F9},
        {"f10", VK_F10},
        {"f11", VK_F11},
        {"f12", VK_F12},
    };
    static std::regex line("([^:]+):\\s*(.+)");

    std::string l;
    while (std::getline(ifs, l)) {
        std::smatch mline;
        std::regex_match(l, mline, line);
        if (mline.size()) {
            std::string keys = mline[1];
            for (char& c : keys)
                c = std::tolower(c);

            std::smatch mkeys;
            auto match_keys = [&keys, &mkeys](auto& r) {
                std::regex_match(keys, mkeys, r);
                return !mkeys.empty();
            };

            Key key{};
            Split(keys, "+", [&](std::string k) {
                if (k == "ctrl")
                    key.ctrl = 1;
                else if (k == "alt")
                    key.alt = 1;
                else if (k == "shift")
                    key.shift = 1;
                else {
                    if (k.size() == 1) {
                        key.code = std::toupper(k[0]);
                    }
                    else {
                        auto i = keymap.find(k);
                        if (i != keymap.end())
                            key.code = i->second;
                    }
                }
                });

            if (key.code)
                body(key, mline[2].str());
        }
    }
    return ret;
}

} // namespace mr
