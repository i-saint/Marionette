#include "pch.h"
#include "MouseReplayer.h"

namespace mr {

class Player : public IPlayer
{
public:
    void release() override;
    bool start(uint32_t loop) override;
    bool stop() override;
    bool isPlaying() const override;
    bool update() override;
    bool load(const char* path) override;

private:
    bool m_playing = false;
    millisec m_time_start = 0;
    uint32_t m_record_index = 0;
    uint32_t m_loop_required = 0, m_loop_count = 0;
    std::vector<OpRecord> m_records;
};


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
            rec.execute();
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

bool Player::load(const char* path)
{
    m_records.clear();

    std::ifstream ifs(path, std::ios::in);
    if (!ifs)
        return false;

    std::string l;
    while (std::getline(ifs, l)) {
        OpRecord rec;
        if (rec.fromText(l))
            m_records.push_back(rec);
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
