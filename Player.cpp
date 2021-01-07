#include "pch.h"
#include "MouseReplayer.h"


bool Player::startReplay(uint32_t loop)
{
    if (m_playing)
        return false;

    m_time_start = NowMS();
    m_loop_required = loop;
    m_loop_current = 0;
    m_record_index = 0;
    m_playing = true;
    return true;
}

bool Player::stopReplay()
{
    if (!m_playing)
        return false;

    m_playing = false;
    return true;
}

bool Player::update()
{
    if (!m_playing || m_records.empty() || m_loop_current >= m_loop_required)
        return false;

    millisec now = NowMS() - m_time_start;
    // execute records
    for (;;) {
        const auto& rec = m_records[m_record_index];
        if (now >= rec.time) {
            rec.execute();
            ++m_record_index;
            DbgPrint("record executed: %s\n", rec.toText().c_str());

            if (m_record_index == m_records.size()) {
                // go next loop or stop
                ++m_loop_current;
                m_record_index = 0;
                m_time_start = NowMS();
                break;
            }
        }
        else {
            break;
        }
    }

    // stop if escape key is pressed
    if (::GetKeyState(VK_ESCAPE) & 0x80) {
        m_playing = false;
        return false;
    }

    SleepMS(1);
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
