#include "pch.h"
#include "Test.h"
#include "Marionette.h"

using mr::unorm8;
using mr::int2;
using mr::float2;
using mr::float4;
using mr::Rect;

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

mr::IReduceMinMax::Result MinMax_Reference(mr::ITexture2DPtr src, int2 range = {})
{
    if (range.x == 0)
        range = src->getSize();

    auto reduce = [&src, range]<class T, class U>(T& vmin, T& vmax, int2& pmin, int2& pmax, const U * data_, int pitch) {
        vmin = vmax = data_[0];
        for (int y = 0; y < range.y; ++y) {
            auto data = (const U*)((const byte*)data_ + (pitch * y));
            for (int x = 0; x < range.x; ++x) {
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

void DrawCircle(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr dst, int2 pos, float radius, float border, float4 color)
{
    auto filter = gfx->createShape();
    filter->setDst(dst);
    filter->addCircle(pos, radius, border, color);
    filter->dispatch();
}

void DrawRect(mr::IGfxInterfacePtr gfx, mr::ITexture2DPtr dst, Rect rect, float border, float4 color)
{
    auto filter = gfx->createShape();
    filter->setDst(dst);
    filter->addRect(rect, border, color);
    filter->dispatch();
}

testCase(Filter)
{
    mr::CaptureMonitor(mr::GetPrimaryMonitor(), [](const void* data, int w, int h) {
        mr::SaveAsPNG("EntireScreen.png", w, h, mr::PixelFormat::BGRAu8, data, 0, true);
        });

    std::vector<std::future<bool>> async_ops;
    auto wait_async_ops = [&]() {
        for (auto& a : async_ops)
            a.wait();
        async_ops.clear();
    };

    const float scale = 0.5f;
    const int contour_block_size = 3;
    const int expand_block_size = 3;
    const float binarize_threshold = 0.2f;

    auto gfx = mr::CreateGfxInterface();

    mr::ITexture2DPtr tmp_image, tmp_mask;
    int2 tmp_size{};
    uint32_t tmp_bits{};
    tmp_image = gfx->createTextureFromFile("template.png");
    if (tmp_image) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        auto filter = mr::CreateFilterSet(gfx);
        mr::ITexture2DPtr src, rtrans, rcont, rbin, rexp;
        src = rtrans = filter->transform(tmp_image, scale, true);
        src = rcont = filter->contour(src, contour_block_size);
        src =
            rbin = filter->binarize(src, binarize_threshold);
        tmp_image = src;
        tmp_mask =
            rexp = filter->expand(src, expand_block_size);

        async_ops.push_back(rcont->saveAsync("template_contour.png"));
        async_ops.push_back(rbin->saveAsync("template_binary.png"));
        async_ops.push_back(rexp->saveAsync("template_binary_expand.png"));

        {
            auto rce = mr::CreateFilterSet(gfx)->expand(rcont, expand_block_size);
            async_ops.push_back(rce->saveAsync("template_contour_expand.png"));
        }

        testPrint("Total (ref): %f\n", Total_Reference(rcont).valf);
        testPrint("Total  (cs): %f\n", filter->total(rcont).get().valf);

        tmp_size = tmp_image->getSize();
        tmp_bits = filter->countBits(rexp).get();
        testPrint("CountBits (ref): %d\n", CountBits_Reference(rexp));
        testPrint("CountBits  (cs): %d\n", tmp_bits);
        testPrint("\n");
    }

    auto tex = gfx->createTextureFromFile("EntireScreen.png");

    if (tex) {
        // downscale filter test
        auto with_filter = mr::CreateFilterSet(gfx)->transform(tex, 0.25f, false, true);
        auto without_filter = mr::CreateFilterSet(gfx)->transform(tex, 0.25f, false, false);
        async_ops.push_back(with_filter->saveAsync("EntireScreen_half_with_filter.png"));
        async_ops.push_back(without_filter->saveAsync("EntireScreen_half_without_filter.png"));
    }
    wait_async_ops();

    if (tex) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        mr::ITexture2DPtr src, rtrans, rcont, rbin, rmatch;
        auto filter = mr::CreateFilterSet(gfx);

        auto time_begin = test::Now();

        src = rtrans = filter->transform(tex, scale, true);
        src = rcont = filter->contour(src, contour_block_size);
        src =
            rbin = filter->binarize(src, binarize_threshold);

        int2 offset = { 0, 0 };
        int2 range = src->getSize() - offset - tmp_size;
        if (tmp_image) {
            src = filter->match(src, tmp_image, tmp_mask, {offset, range}, false);

            float denom{};
            if (tmp_image->getFormat() == mr::TextureFormat::Binary)
                denom = float(tmp_mask ? tmp_bits : uint32_t(tmp_size.x * tmp_size.y));
            else
                denom = tmp_size.x * tmp_size.y;
            src = rmatch = filter->normalize(src, denom);
        }

        auto result = filter->minmax(src, range).get();

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
            print(MinMax_Reference(rmatch, range));

            testPrint("MinMax  (cs):\n");
            print(result);

            auto marked = mr::CreateFilterSet(gfx)->copy(tex);
            auto pos = int2(float2(result.pos_min + offset) / scale);
            auto size = int2(float2(tmp_size) / scale);
            DrawRect(gfx, marked, Rect{ pos, size }, 2, { 1.0f, 0.0f, 0.0f, 1.0f });

            //auto center = pos + size / 2;
            //auto radius = float(std::max(size.x, size.y)) / 2.0f;
            //DrawCircle(gfx, marked, center, radius, 2, { 1.0f, 0.0f, 0.0f, 1.0f });

            async_ops.push_back(marked->saveAsync("result.png"));
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
    wait_async_ops();
}

testCase(ScreenCapture)
{
    std::vector<std::future<bool>> async_ops;
    auto wait_async_ops = [&]() {
        for (auto& a : async_ops)
            a.wait();
        async_ops.clear();
    };

    auto gfx = mr::CreateGfxInterface();
    auto filter = mr::CreateFilterSet(gfx);

    auto time_start = mr::NowNS();
    auto scap = gfx->createScreenCapture();
    testExpect(scap != nullptr);

    if (scap->startCapture(mr::GetPrimaryMonitor())) {
        for (int i = 0; i < 5; ++i) {
            auto frame = scap->waitNextFrame();
            if (!frame.surface)
                continue;

            testPrint("frame %d [%.2f ms]\n", i, float(double(frame.present_time - time_start) / 1000000.0));
            auto surface = filter->copy(frame.surface, frame.size, mr::TextureFormat::RGBAu8);

            char filename[256];
            snprintf(filename, std::size(filename), "Frame%02d.png", i);
            async_ops.push_back(surface->saveAsync(filename));
        }
        scap->stopCapture();
    }
    wait_async_ops();
}

testCase(ScreenMatcher)
{
    auto gfx = mr::CreateGfxInterface();
    auto matcher = mr::CreateScreenMatcher(gfx);
    testExpect(matcher != nullptr);

    auto tmpl = matcher->createTemplate("template.png");
    testExpect(tmpl != nullptr);

    auto primary_monitor = mr::GetPrimaryMonitor();
    mr::IScreenMatcher::Result last_result;

    ::Sleep(2000);
    auto time_start = mr::NowNS();
    for (int i = 0; i < 10; ++i) {
        auto time_begin_match = mr::NowNS();
        //auto result = matcher->match(tmpl, primary_monitor);
        auto result = matcher->match(tmpl, ::GetForegroundWindow());
        auto time_end_match = mr::NowNS();
        auto elapsed = float(double(time_end_match - time_begin_match) / 1000000.0);

        auto pos = result.region.pos;
        testPrint("frame %d [%.2f ms]: score %.4f (%d, %d)\n", i, elapsed, result.score, pos.x, pos.y);
        last_result = result;
    }


    {
        auto marked = mr::CreateFilterSet(gfx)->copy(last_result.surface, mr::TextureFormat::RGBAu8);
        auto region = last_result.region;
        DrawRect(gfx, marked, region, 2, { 1.0f, 0.0f, 0.0f, 1.0f });
        marked->save("ScreenMatcher.png");

        auto score = mr::CreateFilterSet(gfx)->normalize(last_result.match_result, tmpl->getMaskBits());
        score->save("ScreenMatcher_score.png");
    }
}
