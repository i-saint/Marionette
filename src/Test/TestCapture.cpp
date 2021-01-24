#include "pch.h"
#include "Test.h"
#include "MouseReplayer.h"

static void SetDllSearchPath()
{
    auto bin_path = mr::GetCurrentModuleDirectory() + "\\bin";
    ::SetDllDirectoryA(bin_path.c_str());
}

testRegisterInitializer(mr,
    []() { SetDllSearchPath(); ::mr::Initialize(); },
    []() { ::mr::Finalize(); });


TestCase(Image)
{
    mr::CaptureEntireScreen([](const void* data, int w, int h) {
        mr::SaveAsPNG("EntireScreen.png", w, h, mr::PixelFormat::BGRAu8, data, 0, true);
        });
}

TestCase(Texture)
{
    auto gfx = mr::CreateGfxInterfaceShared();

    if (auto tex = gfx->createTextureFromFile("EntireScreen.png")) {
        tex->save("TextureSave.png");
    }
}

TestCase(ScreenCapture)
{
    auto gfx = mr::CreateGfxInterfaceShared();

    std::mutex mutex;
    std::condition_variable cond;

    auto on_frame = [&cond](auto& frame) {
        cond.notify_one();
    };

    auto scap = gfx->createScreenCapture();
    scap->setOnFrameArrived(on_frame);
    if (scap->startCapture(mr::GetPrimaryMonitor())) {
        for (int i = 0; i < 5; ++i) {
            std::unique_lock<std::mutex> lock(mutex);
            cond.wait(lock);

            auto frame = scap->getFrame();
            char filename[256];
            snprintf(filename, std::size(filename), "Frame%02d.png", i);
            frame.surface->save(filename);
        }
        scap->stopCapture();
    }
}
