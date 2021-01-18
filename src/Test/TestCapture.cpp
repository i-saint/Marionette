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

TestCase(GraphicsCapture)
{
    auto capture = mr::CreateGraphicsCaptureShared();

    mr::IGraphicsCapture::Options opt;
    opt.free_threaded = true;
    capture->setOptions(opt);

    std::mutex mutex;
    std::condition_variable cond;

    auto task = [&capture, &cond](ID3D11Texture2D* surface) {
        mrProfile("GraphicsCapture");
        capture->getPixels([capture](const byte* data, int width, int height, int pitch) {
            auto image = mr::MakeCVImage(data, width, height, pitch);
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
