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
    mr::CaptureMonitor(mr::GetPrimaryMonitor(), [](const void* data, int w, int h) {
        mr::SaveAsPNG("EntireScreen.png", w, h, mr::PixelFormat::BGRAu8, data, 0, true);
        });

    std::vector<std::future<bool>> async_ops;

    auto gfx = mr::CreateGfxInterfaceShared();

    mr::ITexture2DPtr template_image = gfx->createTextureFromFile("template.png");
    if (template_image) {
        mr::TransformParams trans;
        trans.src = template_image;
        trans.grayscale = true;
        gfx->transform(trans);

        mr::ContourParams cont;
        cont.src = trans.dst;
        gfx->contour(cont);
        std::swap(cont.dst, template_image);
        async_ops.push_back(template_image->saveAsync("template_contour.png"));
    }

    auto tex = gfx->createTextureFromFile("EntireScreen.png");
    if (tex) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        auto time_begin = test::Now();
        mr::ITexture2DPtr match_result;
        mr::ITexture2DPtr src;

        mr::TransformParams trans;
        trans.src = tex;
        trans.grayscale = true;
        gfx->transform(trans);
        src = trans.dst;

        mr::ContourParams cont;
        cont.src = src;
        gfx->contour(cont);
        src = cont.dst;

        if (template_image) {
            mr::TemplateMatchParams tmatch;
            tmatch.src = src;
            tmatch.template_image = template_image;
            gfx->templateMatch(tmatch);
            src = match_result = tmatch.dst;
        }

        mr::ReduceMinmaxParams red;
        red.src = src;
        gfx->reduceMinMax(red);

        auto result = red.result.get();

        auto elapsed = test::Now() - time_begin;
        testPrint("elapsed: %.2f\n", test::NS2MS(elapsed));

        testPrint("Min: %f (%d, %d)\n", result.val_min, result.pos_min.x, result.pos_min.y);
        testPrint("Max: %f (%d, %d)\n", result.val_max, result.pos_max.x, result.pos_max.y);

        async_ops.push_back(trans.dst->saveAsync("grayscale.png"));
        async_ops.push_back(cont.dst->saveAsync("contour.png"));
        if (match_result)
            async_ops.push_back(match_result->saveAsync("match.png"));
    }
    for (auto& a : async_ops)
        a.wait();
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
