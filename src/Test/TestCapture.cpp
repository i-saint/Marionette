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

    auto gfx = mr::CreateGfxInterface();

    mr::ITexture2DPtr template_image = gfx->createTextureFromFile("template.png");
    if (template_image) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        auto trans = gfx->createTransform();
        trans->setSrc(template_image);
        trans->setGrayscale(true);
        //trans->setScale(0.5f);
        trans->dispatch();

        auto contour = gfx->createContour();
        contour->setSrc(trans->getDst());
        contour->dispatch();

        template_image = contour->getDst();

        async_ops.push_back(template_image->saveAsync("template_contour.png"));
    }

    auto tex = gfx->createTextureFromFile("EntireScreen.png");
    if (tex) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        auto time_begin = test::Now();
        mr::ITexture2DPtr src;

        auto trans = gfx->createTransform();
        trans->setSrc(tex);
        trans->setGrayscale(true);
        //trans->setScale(0.5f);
        trans->dispatch();
        src = trans->getDst();

        auto contour = gfx->createContour();
        contour->setSrc(src);
        contour->setBlockSize(5);
        contour->dispatch();
        src = contour->getDst();

        auto binarize = gfx->createBinarize();
        binarize->setSrc(src);
        binarize->setThreshold(0.10f);
        binarize->dispatch();
        //src = binarize->getDst();

        auto tmatch = gfx->createTemplateMatch();
        if (template_image) {
            tmatch->setSrc(src);
            tmatch->setTemplate(template_image);
            tmatch->dispatch();
            src = tmatch->getDst();
        }

        auto rminmax = gfx->createReduceMinMax();
        rminmax->setSrc(src);
        rminmax->dispatch();

        auto result = rminmax->getResult().get();

        auto elapsed = test::Now() - time_begin;
        testPrint("elapsed: %.2f ms\n", test::NS2MS(elapsed));

        testPrint("Min: %f (%d, %d)\n", result.val_min, result.pos_min.x, result.pos_min.y);
        testPrint("Max: %f (%d, %d)\n", result.val_max, result.pos_max.x, result.pos_max.y);

        if (auto rtrans = trans->getDst())
            async_ops.push_back(rtrans->saveAsync("grayscale.png"));
        if (auto rcont = contour->getDst())
            async_ops.push_back(rcont->saveAsync("contour.png"));
        if (auto rbin = binarize->getDst())
            async_ops.push_back(rbin->saveAsync("binarize.png"));
        if (auto rtmatch = tmatch->getDst())
            async_ops.push_back(rtmatch->saveAsync("match.png"));
    }
    for (auto& a : async_ops)
        a.wait();
}

TestCase(ScreenCapture)
{
    auto gfx = mr::CreateGfxInterface();

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
