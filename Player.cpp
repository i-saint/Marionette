#include "pch.h"
#include "MouseReplayer.h"

namespace mr {

#ifdef mrWithOpenCV
class ImageManager
{
public:
    static ImageManager& instance();

    int fetch(const std::string& path);
    cv::Mat* get(int handle);

private:
    struct Record
    {
        std::string path;
        cv::Mat image;
        std::future<void> load;
    };
    using RecordPtr = std::unique_ptr<Record>;
    int m_seed = 0;
    std::map<int, RecordPtr> m_records;
};
#endif

class Player : public IPlayer
{
public:
    void release() override;
    bool start(uint32_t loop) override;
    bool stop() override;
    bool isPlaying() const override;
    bool update() override;
    bool load(const char* path) override;

    void execRecord(const OpRecord& rec);

private:
    bool m_playing = false;
    millisec m_time_start = 0;
    uint32_t m_record_index = 0;
    uint32_t m_loop_required = 0, m_loop_count = 0;
    std::vector<OpRecord> m_records;

    struct State
    {
        int x = 0, y = 0;
    };
    State m_state;
    std::vector<State> m_state_stack;
};


#ifdef mrWithOpenCV

ImageManager& ImageManager::instance()
{
    static ImageManager s_inst;
    return s_inst;
}

int ImageManager::fetch(const std::string& path)
{
    for (auto& kvp : m_records) {
        if (kvp.second->path == path)
            return kvp.first;
    }

    auto rec = std::make_unique<Record>();
    auto p = rec.get();
    rec->path = path;
    rec->load = std::async(std::launch::async, [p]() {
        p->image = cv::imread(p->path, cv::IMREAD_GRAYSCALE);
        });

    int handle = ++m_seed;
    m_records[handle] = std::move(rec);
    return handle;
}

cv::Mat* ImageManager::get(int handle)
{
    auto& i = m_records.find(handle);
    if (i != m_records.end()) {
        if (i->second->load.valid()) {
            i->second->load.get();
            i->second->load = {};
        }
        return &i->second->image;
    }
    return nullptr;
}

#endif



void Player::release()
{
    delete this;
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


    millisec now = NowMS() - m_time_start;
    // execute records
    for (;;) {
        const auto& rec = m_records[m_record_index];
        if (now >= rec.time) {
            execRecord(rec);
            DbgPrint("record executed: %s\n", rec.toText().c_str());
            ++m_record_index;

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
    auto MakeMouseMove = [](INPUT& input, int x, int y) {
        // http://msdn.microsoft.com/en-us/library/ms646260(VS.85).aspx
        // If MOUSEEVENTF_ABSOLUTE value is specified, dx and dy contain normalized absolute coordinates between 0 and 65,535.
        // The event procedure maps these coordinates onto the display surface.
        // Coordinate (0,0) maps onto the upper-left corner of the display surface, (65535,65535) maps onto the lower-right corner.

        LONG screen_width = ::GetSystemMetrics(SM_CXSCREEN) - 1; // SM_CXVIRTUALSCREEN
        LONG screen_height = ::GetSystemMetrics(SM_CYSCREEN) - 1; // SM_CYVIRTUALSCREEN
        input.mi.dx = (LONG)(x * (65535.0f / screen_width));
        input.mi.dy = (LONG)(y * (65535.0f / screen_height));
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    };

    switch (rec.type)
    {
    case OpType::PushState:
    {
        m_state_stack.push_back(m_state);
        break;
    }

    case OpType::PopState:
    {
        if (!m_state_stack.empty()) {
            m_state = m_state_stack.back();
            m_state_stack.pop_back();

            INPUT input{};
            input.type = INPUT_MOUSE;
            MakeMouseMove(input, m_state.x, m_state.y);
            ::SendInput(1, &input, sizeof(INPUT));
        }
        break;
    }

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
            m_state.x = rec.data.mouse.x;
            m_state.y = rec.data.mouse.y;
            MakeMouseMove(input, m_state.x, m_state.y);
        }
        else if (rec.type == OpType::MouseMoveRel) {
            m_state.x += rec.data.mouse.x;
            m_state.y += rec.data.mouse.y;
            MakeMouseMove(input, m_state.x, m_state.y);
        }
        else if (rec.type == OpType::MouseMoveMatch) {
#ifdef mrWithOpenCV
            auto image = ImageManager::instance().get(rec.data.mouse.image_handle);
            if (image) {
                bool match;
                int x, y;
                std::tie(match, x, y) = MatchImage(nullptr, *image);
                if (match) {
                    m_state.x = x;
                    m_state.y = y;
                    MakeMouseMove(input, m_state.x, m_state.y);
                }
            }
#else
            break;
#endif
        }
        ::SendInput(1, &input, sizeof(INPUT));
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
#ifdef mrWithOpenCV
            if (rec.type == OpType::MouseMoveMatch) {
                rec.data.mouse.image_handle = ImageManager::instance().fetch(rec.data.mouse.image_path);
            }
#endif
            m_records.push_back(rec);
        }
    }
    std::stable_sort(m_records.begin(), m_records.end(),
        [](auto& a, auto& b) { return a.time < b.time; });

    return !m_records.empty();
}

mrAPI IPlayer* CreatePlayer()
{
    return new Player();
}

} // namespace mr
