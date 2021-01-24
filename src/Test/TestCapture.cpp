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


TestCase(Filter)
{
    mr::CaptureEntireScreen([](const void* data, int w, int h) {
        mr::SaveAsPNG("EntireScreen.png", w, h, mr::PixelFormat::BGRAu8, data, 0, true);
        });

    auto gfx = mr::CreateGfxInterfaceShared();
    auto tex = gfx->createTextureFromFile("EntireScreen.png");
    if (tex) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        mr::TransformParams trans;
        trans.src = tex;
        trans.grayscale = true;
        gfx->transform(trans);

        mr::ContourParams cont;
        cont.src = trans.dst;
        gfx->contour(cont);

        mr::ReduceMinmaxParams red;
        red.src = cont.dst;
        gfx->reduceMinMax(red);

        auto save1 = trans.dst->saveAsync("grayscale.png");
        auto save2 = cont.dst->saveAsync("contour.png");
        auto result = red.result.get();
        testPrint("Min: %f (%d, %d)\n", result.val_min, result.pos_min.x, result.pos_min.y);
        testPrint("Max: %f (%d, %d)\n", result.val_max, result.pos_max.x, result.pos_max.y);
        save1.wait();
        save2.wait();
    }
}

TestCase(ScreenCapture)
{
    auto gfx = mr::CreateGfxInterfaceShared();

    std::mutex mutex;
    std::condition_variable cond;
    std::atomic<bool> ready{};
    auto on_frame = [&cond, &ready](auto& frame) {
        ready = true;
        cond.notify_one();
    };

    mr::ITexture2DPtr tex;

    auto scap = gfx->createScreenCapture();
    if (scap && scap->startCapture(mr::GetPrimaryMonitor())) {
        scap->setOnFrameArrived(on_frame);
        for (int i = 0; i < 5; ++i) {
            std::unique_lock<std::mutex> lock(mutex);
            ready = false;
            cond.wait(lock, [&ready]() { return ready.load(); });
            if (!ready)
                continue;

            auto frame = scap->getFrame();
            testPrint("frame %d [%f]\n", i, float(double(frame.present_time) / 1000000.0));
            tex = frame.surface;

            //char filename[256];
            //snprintf(filename, std::size(filename), "Frame%02d.png", i);
            //frame.surface->save(filename);
        }
        scap->stopCapture();
    }

}
