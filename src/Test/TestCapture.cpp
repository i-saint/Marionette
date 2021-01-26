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



uint32_t CountBits(const uint32_t* data, size_t size)
{
    uint32_t ret{};
    for (auto v : std::span{ data, data + size })
        ret += std::popcount(v);
    return ret;
}

mr::ITexture2DPtr Transform(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src, float scale, bool grayscale)
{
    auto filter = gfx->createTransform();
    filter->setSrc(src);
    filter->setGrayscale(grayscale);
    filter->setScale(scale);
    filter->dispatch();
    return filter->getDst();
}

mr::ITexture2DPtr Contour(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src, int block_size = 5)
{
    auto filter = gfx->createContour();
    filter->setSrc(src);
    filter->setBlockSize(block_size);
    filter->dispatch();
    return filter->getDst();
}

mr::ITexture2DPtr Binarize(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src, float threshold = 0.1f)
{
    auto filter = gfx->createBinarize();
    filter->setSrc(src);
    filter->setThreshold(threshold);
    filter->dispatch();
    return filter->getDst();
}

mr::ITexture2DPtr TemplateMatch(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src, mr::ITexture2DPtr tmp)
{
    auto filter = gfx->createTemplateMatch();
    filter->setSrc(src);
    filter->setTemplate(tmp);
    filter->dispatch();
    return filter->getDst();
}

std::future<float> Total(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src)
{
    auto filter = gfx->createReduceTotal();
    filter->setSrc(src);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}

std::future<uint32_t> CountBits(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src)
{
    auto filter = gfx->createReduceCountBits();
    filter->setSrc(src);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}

std::future<mr::IReduceMinMax::Result> MinMax(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src)
{
    auto filter = gfx->createReduceMinMax();
    filter->setSrc(src);
    filter->dispatch();
    return std::async(std::launch::deferred,
        [filter]() mutable { return filter->getResult(); });
}

TestCase(Filter)
{
    mr::CaptureMonitor(mr::GetPrimaryMonitor(), [](const void* data, int w, int h) {
        mr::SaveAsPNG("EntireScreen.png", w, h, mr::PixelFormat::BGRAu8, data, 0, true);
        });

    std::vector<std::future<bool>> async_ops;
    const float scale = 1.0f;
    const int contour_block_size = 5;
    const float binarize_threshold = 0.1f;

    auto gfx = mr::CreateGfxInterface();

    auto template_image = gfx->createTextureFromFile("template.png");
    if (template_image) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        mr::ITexture2DPtr src, rtrans, rcont, rbin;
        src = rtrans = Transform(gfx, template_image, scale, true);
        src = rcont = Contour(gfx, src, contour_block_size);
        /*src =*/ rbin = Binarize(gfx, src, binarize_threshold);
        template_image = src;

        async_ops.push_back(rcont->saveAsync("template_contour.png"));
        async_ops.push_back(rbin->saveAsync("template_binary.png"));

        uint32_t cbits1 = CountBits(gfx, rbin).get();
        uint32_t cbits2{};
        rbin->read([&cbits2, &rbin](const void* data_, int pitch) {
            auto size = rbin->getSize();
            for (int i = 0; i < size.y; ++i) {
                auto data = (const uint32_t*)((const byte*)data_ + (pitch * i));
                cbits2 += CountBits(data, size.x);
            }
            });
        testPrint("cbits1: %d\n", cbits1);
        testPrint("cbits2: %d\n", cbits2);
    }

    auto tex = gfx->createTextureFromFile("EntireScreen.png");
    if (tex) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        auto time_begin = test::Now();

        mr::ITexture2DPtr src, rtrans, rcont, rbin, rmatch;

        src = rtrans = Transform(gfx, tex, scale, true);
        src = rcont = Contour(gfx, src, contour_block_size);
        /*src =*/ rbin = Binarize(gfx, src, binarize_threshold);

        if (template_image) {
            src = rmatch = TemplateMatch(gfx, src, template_image);
        }

        auto rminmax = gfx->createReduceMinMax();
        rminmax->setSrc(src);
        rminmax->dispatch();

        auto result = rminmax->getResult();

        auto elapsed = test::Now() - time_begin;
        testPrint("elapsed: %.2f ms\n", test::NS2MS(elapsed));

        testPrint("Min: %f (%d, %d)\n", result.val_min, result.pos_min.x, result.pos_min.y);
        testPrint("Max: %f (%d, %d)\n", result.val_max, result.pos_max.x, result.pos_max.y);

        if (rtrans)
            async_ops.push_back(rtrans->saveAsync("grayscale.png"));
        if (rcont)
            async_ops.push_back(rcont->saveAsync("contour.png"));
        if (rbin)
            async_ops.push_back(rbin->saveAsync("binarize.png"));
        if (rmatch)
            async_ops.push_back(rmatch->saveAsync("match.png"));
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
