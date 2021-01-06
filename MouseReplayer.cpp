#include "pch.h"
#include "MouseReplayer.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        puts("usage: MousePlayer.exe path_to_replay_file");
        return 0;
    }

    auto path = argv[1];
    int loop = 1;
    if (argc >= 3)
        loop = std::max<int>(1, std::atoi(argv[2]));

    Player player;
    if (!player.load(path))
        return 1;
    player.startReplay(loop);
    while (player.update()) {}
    player.stopReplay();
}
