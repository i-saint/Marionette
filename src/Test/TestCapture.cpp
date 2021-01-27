#include "pch.h"
#include "Test.h"
#include "Marionette.h"

using mr::unorm8;
using mr::int2;

static void SetDllSearchPath()
{
    auto bin_path = mr::GetCurrentModuleDirectory() + "\\bin";
    ::SetDllDirectoryA(bin_path.c_str());
}

testRegisterInitializer(mr,
    []() { SetDllSearchPath(); ::mr::Initialize(); },
    []() { ::mr::Finalize(); });



mr::IReduceTotal::Result Total_Reference(mr::ITexture2DPtr src)
{
    auto reduce = [&src]<class T, class U>(T& ret, const U* data_, int pitch) {
        auto size = src->getSize();
        for (int y = 0; y < size.y; ++y) {
            auto data = (const U*)((const byte*)data_ + (pitch * y));
            for (auto v : std::span{ data, data + size.x })
                ret += v;
        }
    };

    mr::IReduceTotal::Result ret{};
    src->read([&](const void* data, int pitch) {
        switch (src->getFormat()) {
        case mr::TextureFormat::Ru8:
            reduce(ret.valf, (const unorm8*)data, pitch);
            break;
        case mr::TextureFormat::Rf32:
            reduce(ret.valf, (const float*)data, pitch);
            break;
        case mr::TextureFormat::Ri32:
            reduce(ret.vali, (const uint32_t*)data, pitch);
            break;
        }
        });
    return ret;
}

uint32_t CountBits_Reference(mr::ITexture2DPtr src)
{
    auto process_line = [](const uint32_t* data, size_t size) {
        uint32_t ret{};
        for (auto v : std::span{ data, data + size })
            ret += std::popcount(v);
        return ret;
    };

    uint32_t ret{};
    src->read([&](const void* data_, int pitch) {
        auto size = src->getSize();
        for (int i = 0; i < size.y; ++i) {
            auto data = (const uint32_t*)((const byte*)data_ + (pitch * i));
            ret += process_line(data, size.x);
        }
        });
    return ret;
}

mr::IReduceMinMax::Result MinMax_Reference(mr::ITexture2DPtr src)
{
    auto reduce = [&src]<class T, class U>(T& vmin, T& vmax, int2& pmin, int2& pmax, const U * data_, int pitch) {
        auto size = src->getSize();
        vmin = vmax = data_[0];
        for (int y = 0; y < size.y; ++y) {
            auto data = (const U*)((const byte*)data_ + (pitch * y));
            for (int x = 0; x < size.x; ++x) {
                auto v = data[x];
                if (v < vmin) {
                    vmin = v;
                    pmin = { x, y };
                }
                if (v > vmax) {
                    vmax = v;
                    pmax = { x, y };
                }
            }
        }
    };

    mr::IReduceMinMax::Result ret{};
    src->read([&](const void* data, int pitch) {
        switch (src->getFormat()) {
        case mr::TextureFormat::Ru8:
            reduce(ret.valf_min, ret.valf_max, ret.pos_min, ret.pos_max, (const unorm8*)data, pitch);
            break;
        case mr::TextureFormat::Rf32:
            reduce(ret.valf_min, ret.valf_max, ret.pos_min, ret.pos_max, (const float*)data, pitch);
            break;
        case mr::TextureFormat::Ri32:
            reduce(ret.vali_min, ret.vali_max, ret.pos_min, ret.pos_max, (const uint32_t*)data, pitch);
            break;
        }
        });
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

mr::ITexture2DPtr Normalize(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src, float max)
{
    auto filter = gfx->createNormalize();
    filter->setSrc(src);
    filter->setMax(max);
    filter->dispatch();
    return filter->getDst();
}

std::future<mr::IReduceTotal::Result> Total(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr src)
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
    const float binarize_threshold = 0.2f;

    auto gfx = mr::CreateGfxInterface();

    auto template_image = gfx->createTextureFromFile("template.png");
    uint32_t template_bits{};
    if (template_image) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        mr::ITexture2DPtr src, rtrans, rcont, rbin;
        src = rtrans = Transform(gfx, template_image, scale, true);
        src = rcont = Contour(gfx, src, contour_block_size);
        src = rbin = Binarize(gfx, src, binarize_threshold);
        template_image = src;

        async_ops.push_back(rcont->saveAsync("template_contour.png"));
        async_ops.push_back(rbin->saveAsync("template_binary.png"));

        testPrint("Total (ref): %f\n", Total_Reference(rcont).valf);
        testPrint("Total  (cs): %f\n", Total(gfx, rcont).get().valf);

        template_bits = CountBits(gfx, rbin).get();
        testPrint("CountBits (ref): %d\n", CountBits_Reference(rbin));
        testPrint("CountBits  (cs): %d\n", template_bits);
        testPrint("\n");
    }

    auto tex = gfx->createTextureFromFile("EntireScreen.png");
    if (tex) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        auto time_begin = test::Now();

        mr::ITexture2DPtr src, rtrans, rcont, rbin, rmatch;

        src = rtrans = Transform(gfx, tex, scale, true);
        src = rcont = Contour(gfx, src, contour_block_size);
        src = rbin = Binarize(gfx, src, binarize_threshold);

        if (template_image) {
            auto s = template_image->getSize();
            src = TemplateMatch(gfx, src, template_image);
            src = rmatch = Normalize(gfx, src, template_bits);
        }

        auto rminmax = gfx->createReduceMinMax();
        rminmax->setSrc(src);
        rminmax->dispatch();

        auto result = rminmax->getResult();

        auto elapsed = test::Now() - time_begin;
        testPrint("elapsed: %.2f ms\n", test::NS2MS(elapsed));
        testPrint("\n");

        if (rmatch) {
            auto print = [&](auto r) {
                if (rmatch->getFormat() == mr::TextureFormat::Ri32) {
                    testPrint("  Min: %d (%d, %d)\n", r.vali_min, r.pos_min.x, r.pos_min.y);
                    testPrint("  Max: %d (%d, %d)\n", r.vali_max, r.pos_max.x, r.pos_max.y);
                }
                else {
                    testPrint("  Min: %f (%d, %d)\n", r.valf_min, r.pos_min.x, r.pos_min.y);
                    testPrint("  Max: %f (%d, %d)\n", r.valf_max, r.pos_max.x, r.pos_max.y);
                }
            };

            testPrint("MinMax (ref):\n");
            print(MinMax_Reference(rmatch));

            testPrint("MinMax  (cs):\n");
            print(result);
        }

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
