#include "pch.h"
#include "mrInternal.h"

namespace mr {

class Player : public RefCount<IPlayer>
{
public:
    Player();
    ~Player();
    bool start(uint32_t loop) override;
    bool stop() override;
    bool isPlaying() const override;
    bool update() override;
    bool load(const char* path) override;
    void setMatchTarget(MatchTarget v) override;

    void execRecord(const OpRecord& rec);

private:
    // shared with all instances
    struct SharedData : public RefCount<IObject>
    {
        IScreenMatcherPtr smatch;
    };
    static SharedData* s_data;

private:
    bool m_playing = false;
    millisec m_time_start = 0;
    uint32_t m_record_index = 0;
    uint32_t m_loop_required = 0, m_loop_count = 0;
    MatchTarget m_match_target = MatchTarget::EntireScreen;
    std::vector<OpRecord> m_records;

    struct State
    {
        int2 mouse_pos{};
    };
    State m_state;
    std::map<int, State> m_mouse_state_slots;
};


Player::SharedData* Player::s_data;

Player::Player()
{
    if (!s_data) {
        s_data = new SharedData();
        s_data->smatch = CreateScreenMatcher();
    }
    s_data->addRef();
}

Player::~Player()
{
    if (s_data->release() == 0) {
        s_data = nullptr;
    }
}

bool Player::start(uint32_t loop)
{
    if (m_playing || m_records.empty())
        return false;

    m_time_start = NowMS();
    m_loop_required = loop;
    m_loop_count = 0;
    m_record_index = 0;
    m_playing = true;

    CURSORINFO ci;
    ci.cbSize = sizeof(ci);
    ::GetCursorInfo(&ci);
    m_state.mouse_pos = (int2&)ci.ptScreenPos;
    return true;
}

bool Player::stop()
{
    if (!m_playing)
        return false;

    m_playing = false;
    return true;
}

bool Player::isPlaying() const
{
    return this && m_playing;
}

bool Player::update()
{
    if (!m_playing || m_records.empty())
        return false;


    // execute records
    for (;;) {
        const auto& rec = m_records[m_record_index];

        millisec time_now = NowMS();
        millisec time_rec = time_now - m_time_start;
        if (time_rec >= rec.time) {
            execRecord(rec);
            if (!m_playing)
                break;
            ++m_record_index;

            // adjust time if MouseMoveMatch because it is very slow and causes input hiccup
            millisec elapsed = NowMS() - time_now;
            if (rec.type == OpType::MouseMoveMatch)
                m_time_start += elapsed;
            mrDbgPrint("record executed (%ld ms): %s\n", elapsed, rec.toText().c_str());

            if (m_record_index == m_records.size()) {
                // go next loop or stop
                ++m_loop_count;
                if (m_loop_count >= m_loop_required) {
                    m_playing = false;
                    return false;
                }
                else {
                    m_record_index = 0;
                    m_time_start = NowMS();
                    break;
                }
            }
        }
        else {
            break;
        }
    }
    return true;
}

void Player::execRecord(const OpRecord& rec)
{
    auto MakeMouseMove = [](INPUT& input, int2 screen_pos) {
        // http://msdn.microsoft.com/en-us/library/ms646260(VS.85).aspx
        // If MOUSEEVENTF_ABSOLUTE value is specified, dx and dy contain normalized absolute coordinates between 0 and 65,535.
        // The event procedure maps these coordinates onto the display surface.
        // Coordinate (0,0) maps onto the upper-left corner of the display surface, (65535,65535) maps onto the lower-right corner.

        static float2 s2c = 65535.0f / float2{
            float(::GetSystemMetrics(SM_CXSCREEN)),
            float(::GetSystemMetrics(SM_CYSCREEN))
        };
        int2 cpos = int2(float2(screen_pos) * s2c);
        input.mi.dx = cpos.x;
        input.mi.dy = cpos.y;
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    };

    switch (rec.type)
    {
    case OpType::MouseDown:
    case OpType::MouseUp:
    case OpType::MouseMoveAbs:
    case OpType::MouseMoveRel:
    case OpType::MouseMoveMatch:
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        if (rec.type == OpType::MouseDown) {
            // handle buttons
            switch (rec.data.mouse.button) {
            case 1: input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN; break;
            case 2: input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN; break;
            case 3: input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN; break;
            default: break;
            }
        }
        else if (rec.type == OpType::MouseUp) {
            // handle buttons
            switch (rec.data.mouse.button) {
            case 1: input.mi.dwFlags |= MOUSEEVENTF_LEFTUP; break;
            case 2: input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP; break;
            case 3: input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP; break;
            default: break;
            }
        }
        else if (rec.type == OpType::MouseMoveAbs) {
            m_state.mouse_pos = rec.data.mouse.pos;
            MakeMouseMove(input, m_state.mouse_pos);
        }
        else if (rec.type == OpType::MouseMoveRel) {
            m_state.mouse_pos = rec.data.mouse.pos;
            MakeMouseMove(input, m_state.mouse_pos);
        }
        else if (rec.type == OpType::MouseMoveMatch) {
            const float score_threshold = 0.3f;
            bool matched = false;

            std::vector<ITemplatePtr> templates;
            for (auto& i : rec.exdata.images)
                if (i.tmpl)
                    templates.push_back(i.tmpl);

            auto match_target = ::GetForegroundWindow();
            auto r = s_data->smatch->match(templates, match_target);
            if (r.score <= score_threshold) {
                m_state.mouse_pos = r.region.getCenter();
                MakeMouseMove(input, m_state.mouse_pos);
                matched = true;
            }

            if (!matched) {
                stop();
                break;
            }
        }
        ::SendInput(1, &input, sizeof(INPUT));
        break;
    }

    case OpType::SaveMousePos:
    {
        m_mouse_state_slots[rec.exdata.save_slot] = m_state;
        break;
    }
    case OpType::LoadMousePos:
    {
        auto i = m_mouse_state_slots.find(rec.exdata.save_slot);
        if (i != m_mouse_state_slots.end()) {
            m_state = i->second;

            INPUT input{};
            input.type = INPUT_MOUSE;
            MakeMouseMove(input, m_state.mouse_pos);

            // it seems single mouse move can't step over display boundary. so SendInput twice.
            ::SendInput(1, &input, sizeof(INPUT));
            ::SendInput(1, &input, sizeof(INPUT));
        }
        break;
    }

    case OpType::KeyDown:
    case OpType::KeyUp:
    {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = (WORD)rec.data.key.code;

        if (rec.type == OpType::KeyUp)
            input.ki.dwFlags |= KEYEVENTF_KEYUP;

        ::SendInput(1, &input, sizeof(INPUT));
        break;
    }

    default:
        break;
    }
}

bool Player::load(const char* path)
{
    m_records.clear();

    std::ifstream ifs(path, std::ios::in);
    if (!ifs)
        return false;

    std::string l;
    while (std::getline(ifs, l)) {
        OpRecord rec;
        if (rec.fromText(l)) {
            if (rec.type == OpType::MouseMoveMatch) {
                for (auto& id : rec.exdata.images) {
                    id.tmpl = s_data->smatch->createTemplate(id.path.c_str());
                    if (!id.tmpl) {
                        mrDbgPrint("*** failed to load template %s ***\n", id.path.c_str());
                    }
                }
            }
            m_records.push_back(rec);
        }
    }
    std::stable_sort(m_records.begin(), m_records.end(),
        [](auto& a, auto& b) { return a.time < b.time; });

    return !m_records.empty();
}

void Player::setMatchTarget(MatchTarget v)
{
    m_match_target = v;
}

mrAPI IPlayer* CreatePlayer_()
{
    return new Player();
}

} // namespace mr
