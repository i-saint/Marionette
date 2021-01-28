#include "pch.h"
#include "mrInternal.h"

namespace mr {

class Recorder : public RefCount<IRecorder>
{
public:
    ~Recorder() override;
    bool start() override;
    bool stop() override;
    bool isRecording() const override;
    bool update() override;
    bool save(const char* path) const override;

    void addRecord(const OpRecord& rec) override;

    // internal

private:
    bool m_recording = false;
    millisec m_time_start = 0;
    int m_handle = 0;

    std::vector<OpRecord> m_records;
};

Recorder::~Recorder()
{
    stop();
}

bool Recorder::start()
{
    if (m_recording)
        return false;

    auto receiver = GetReceiver();
    if (!receiver->valid())
        return false;

    m_handle = receiver->addRecorder(
        [this](OpRecord& rec) {
            addRecord(rec);
            return true;
        });

    m_time_start = NowMS();
    m_recording = true;
    return true;
}

bool Recorder::stop()
{
    if (m_recording) {
        m_recording = false;
        mr::OpRecord rec;
        rec.type = mr::OpType::Wait;
        addRecord(rec);
    }
    if (m_handle) {
        GetReceiver()->removeRecorder(m_handle);
        m_handle = 0;
    }
    return true;
}

bool Recorder::isRecording() const
{
    return this && m_recording;
}

bool Recorder::update()
{
    return m_handle != 0;
}

void Recorder::addRecord(const OpRecord& rec_)
{
    OpRecord rec = rec_;
    if (rec.time == -1) {
        rec.time = NowMS() - m_time_start;
    }
    m_records.push_back(rec);
    mrDbgPrint("record added: %s\n", rec.toText().c_str());
}

bool Recorder::save(const char* path) const
{
    std::ofstream ofs(path, std::ios::out);
    if (!ofs)
        return false;

    for (auto& rec : m_records)
        ofs << rec.toText() << std::endl;
    return true;
}

mrAPI IRecorder* CreateRecorder_()
{
    return new Recorder();
}

} // namespace mr
