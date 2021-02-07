#include "pch.h"
#include "mrInternal.h"

namespace mr {

std::string OpRecord::toText() const
{
    auto templates_to_string = [this]() {
        std::string ret;
        ret += Format(" Threshold:%.2f", exdata.match_threshold);
        switch (exdata.match_pattern) {
        case  ITemplate::MatchPattern::Binary:
            ret += " Pattern:\"Binary\"";
            break;
        case  ITemplate::MatchPattern::Grayscale:
            ret += " Pattern:\"Grayscale\"";
            break;
        default:
            ret += " Pattern:\"BinaryContour\"";
            break;
        }

        for (auto& id : exdata.templates)
            ret += Format(" Template:\"%s\"", id.path.c_str());
        return ret;
    };

    switch (type)
    {
    case OpType::KeyDown:
        return Format("%u: KeyDown %d", time, data.key.code);

    case OpType::KeyUp:
        return Format("%u: KeyUp %d", time, data.key.code);

    case OpType::MouseDown:
        return Format("%u: MouseDown %d", time, data.mouse.button);

    case OpType::MouseUp:
        return Format("%u: MouseUp %d", time, data.mouse.button);

    case OpType::MouseMoveAbs:
        return Format("%u: MouseMoveAbs %d %d", time, data.mouse.pos.x, data.mouse.pos.y);

    case OpType::MouseMoveRel:
        return Format("%u: MouseMoveRel %d %d", time, data.mouse.pos.x, data.mouse.pos.y);

    case OpType::SaveMousePos:
        return Format("%u: SaveMousePos %d", time, exdata.save_slot);

    case OpType::LoadMousePos:
        return Format("%u: LoadMousePos %d", time, exdata.save_slot);

    case OpType::MatchParams:
    {
        std::string ret;
        auto& p = exdata.match_params;
        ret += "MatchParams";
        ret += Format(" Scale:%.2f", p.scale);
        ret += Format(" CareDisplayScale:%s", p.care_display_scale ? "true" : "false");
        ret += Format(" ColorRange:{%.2f,%.2f}", p.color_range.x, p.color_range.y);
        ret += Format(" ContourRadius:%.2f", p.contour_radius);
        ret += Format(" ExpandRadius:%.2f", p.expand_radius);
        ret += Format(" BinarizeThreshold:%.2f", p.binarize_threshold);
        return ret;
    }

    case OpType::MouseMoveMatch:
        return Format("%u: MouseMoveMatch") + templates_to_string();

    case OpType::WaitUntilMatch:
        return Format("%u: WaitUntilMatch") + templates_to_string();

    case OpType::Wait:
        return Format("%u: Wait %d", time, exdata.wait_time);

    default:
        return "";
    }
}

bool OpRecord::fromText(const std::string& v)
{
    type = OpType::Unknown;
    if (v.empty() || v.front() == '\r' || v.front() == '\n' || v.front() == '#')
        return false;

    const char* src = v.c_str();

    auto scan_templates = [this, &src]() {
        ScanKVP(src, [this](std::string k, std::string v) {
            if (k == "Threshold") {
                exdata.match_threshold = ToValue<float>(v);
            }
            else if (k == "Pattern") {
                auto p = ToValue<std::string>(v);
                if (p == "Binary")
                    exdata.match_pattern = ITemplate::MatchPattern::Binary;
                else if (p == "Grayscale")
                    exdata.match_pattern = ITemplate::MatchPattern::Grayscale;
                else
                    exdata.match_pattern = ITemplate::MatchPattern::BinaryContour;
            }
            else if (k == "Template") {
                exdata.templates.push_back({ ToValue<std::string>(v) });
            }
            });
    };

    if (sscanf(src, "TimeShift %d", &exdata.time_shift) == 1)
        type = OpType::TimeShift;
    else if (sscanf(src, "%u: KeyDown %d", &time, &data.key.code) == 2)
        type = OpType::KeyDown;
    else if (sscanf(src, "%u: KeyUp %d", &time, &data.key.code) == 2)
        type = OpType::KeyUp;
    else if (sscanf(src, "%u: MouseDown %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseDown;
    else if (sscanf(src, "%u: MouseUp %d", &time, &data.mouse.button) == 2)
        type = OpType::MouseUp;
    else if (sscanf(src, "%u: MouseMoveAbs %d %d", &time, &data.mouse.pos.x, &data.mouse.pos.y) == 3)
        type = OpType::MouseMoveAbs;
    else if (sscanf(src, "%u: MouseMoveRel %d %d", &time, &data.mouse.pos.x, &data.mouse.pos.y) == 3)
        type = OpType::MouseMoveRel;
    else if (sscanf(src, "%u: SaveMousePos %d", &time, &exdata.save_slot) == 2)
        type = OpType::SaveMousePos;
    else if (sscanf(src, "%u: LoadMousePos %d", &time, &exdata.save_slot) == 2)
        type = OpType::LoadMousePos;
    else if (std::strstr(src, "MatchParams ")) {
        type = OpType::MatchParams;
        ScanKVP(src, [this](std::string k, std::string v) {
            auto& p = exdata.match_params;
            if (k == "Scale")
                p.scale = ToValue<float>(v);
            else if (k == "CareDisplayScale")
                p.care_display_scale = ToValue<bool>(v);
            else if (k == "ColorRange")
                p.color_range = ToValue<float2>(v);
            else if (k == "ContourRadius")
                p.contour_radius = ToValue<float>(v);
            else if (k == "ExpandRadius")
                p.expand_radius = ToValue<float>(v);
            else if (k == "BinarizeThreshold")
                p.binarize_threshold = ToValue<float>(v);
            });
    }
    else if (std::strstr(src, "MouseMoveMatch") && sscanf(src, "%u: ", &time) == 1) {
        type = OpType::MouseMoveMatch;
        scan_templates();
    }
    else if (std::strstr(src, "WaitUntilMatch") && sscanf(src, "%u: ", &time) == 1) {
        type = OpType::WaitUntilMatch;
        scan_templates();
    }
    else if (sscanf(src, "%u: Wait %d", &time, &exdata.wait_time) == 2) {
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
