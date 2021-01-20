#include "pch.h"
#include "Test.h"

#define mrWithOpenCV
#define mrWithGraphicsCapture
#include <d3d11.h>
#include "MouseReplayer.h"


struct SetDllSearchPath
{
    SetDllSearchPath()
    {
        auto bin_path = mr::GetCurrentModuleDirectory() + "\\bin";
        ::SetDllDirectoryA(bin_path.c_str());
    }
} s_SetDllSearchPath;

TestCase(Image)
{
    auto image = mr::CaptureEntireScreen();
    cv::imwrite("screen.png", image);
}

TestCase(ScreenCapture)
{
    auto capture = mr::CreateScreenCaptureShared();

    mr::IScreenCapture::Options opt;
    opt.free_threaded = true;
    opt.grayscale = true;
    opt.scale_factor = 0.5f;
    capture->setOptions(opt);

    std::mutex mutex;
    std::condition_variable cond;

    auto task = [&](ID3D11Texture2D* surface) {
        mrProfile("GraphicsCapture");
        capture->getPixels([&](const byte* data, int width, int height, int pitch) {
            int ch = opt.grayscale ? 1 : 4;
            auto image = mr::MakeCVImage(data, width, height, pitch, ch);
            cv::imwrite("fg.png", image);
        });
        cond.notify_one();
    };


    {
        std::unique_lock<std::mutex> lock(mutex);
        HWND hwnd = ::GetAncestor(::GetForegroundWindow(), GA_ROOT);
        if (capture->start(hwnd, task)) {
            cond.wait(lock);
            capture->stop();
        }
    }
}
