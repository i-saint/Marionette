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

TestCase(ScreenCapture)
{
    auto gfx = mr::CreateGfxInterfaceShared();

    if (auto tex = gfx->createTextureFromFile("EntireScreen.png")) {
        tex->save("TextureSave.png");
    }

    //auto capture = mr::CreateScreenCaptureShared();

    //mr::IGraphicsCapture::Options opt;
    //opt.free_threaded = true;
    //opt.grayscale = true;
    //opt.scale_factor = 1.0f;
    //capture->setOptions(opt);

    //std::mutex mutex;
    //std::condition_variable cond;

    //auto task = [&](ID3D11Texture2D* surface) {
    //    mrProfile("GraphicsCapture");
    //    capture->getPixels([&](const void* data, int width, int height, int pitch) {
    //        int ch = opt.grayscale ? 1 : 4;
    //        auto image = mr::MakeCVImage(data, width, height, pitch, ch);
    //        cv::imwrite("fg.png", image);
    //    });
    //    cond.notify_one();
    //};


    //{
    //    std::unique_lock<std::mutex> lock(mutex);
    //    HWND hwnd = ::GetAncestor(::GetForegroundWindow(), GA_ROOT);
    //    if (capture->start(hwnd, task)) {
    //        cond.wait(lock);
    //        capture->stop();
    //    }
    //}
}
