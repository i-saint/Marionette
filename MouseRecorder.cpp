#include "pch.h"
#include "MouseReplayer.h"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        puts("usage: MouseRecorder.exe path_to_replay_file");
        return 0;
    }
    auto path = argv[1];

    auto recorder = mr::CreateRecorderShared();
    if (!recorder->start())
        return 1;
    while (recorder->update()) {}
    recorder->stop();
    recorder->save(path);
}
