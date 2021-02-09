#include "pch.h"
#include "Test.h"
#include "Marionette.h"

using mr::unorm8;
using mr::unorm8x4;
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
            ret += process_line(data, mr::ceildiv(size.x, 32));
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
    auto filter = mr::GetGfxInterface()->createShape();
    filter->setDst(dst);
    filter->addCircle(pos, radius, border, color);
    filter->dispatch();
}

void DrawRect(mr::ITexture2DPtr dst, Rect rect, float border, float4 color)
{
    auto filter = mr::GetGfxInterface()->createShape();
    filter->setDst(dst);
    filter->addRect(rect, border, color);
    filter->dispatch();
}

testCase(Filter)
{
    std::vector<std::future<bool>> async_ops;
    auto wait_async_ops = [&]() {
        for (auto& a : async_ops)
            a.wait();
        async_ops.clear();
    };

    mr::CaptureMonitor(mr::GetPrimaryMonitor(), [](const void* data, int w, int h) {
        mr::SaveAsPNG("Screen.png", w, h, mr::PixelFormat::BGRAu8, data, 0, true);
        });

    const float scale = 0.5f;
    const float contour_radius = 1.0f;
    const float expand_radius = 1.0f;
    const float binarize_threshold = 0.2f;

    auto gfx = mr::GetGfxInterface();
    auto filter = mr::CreateFilterSet();

    mr::ITexture2DPtr tmp_image, tmp_gray, tmp_contour, tmp_bin, tmp_mask;
    int2 tmp_size{};
    uint32_t tmp_bits{};
    tmp_image = gfx->createTextureFromFile("template.png");
    if (tmp_image) {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        int2 size = int2(float2(tmp_image->getSize()) * scale);

        tmp_gray = gfx->createTexture(size.x, size.y, mr::TextureFormat::Ru8);
        tmp_contour = gfx->createTexture(size.x, size.y, mr::TextureFormat::Ru8);
        tmp_bin = gfx->createTexture(size.x, size.y, mr::TextureFormat::Binary);
        tmp_mask = gfx->createTexture(size.x, size.y, mr::TextureFormat::Binary);

        filter->transform(tmp_gray, tmp_image, scale, true);
        filter->contour(tmp_contour, tmp_gray, contour_radius);
        filter->binarize(tmp_bin, tmp_contour, binarize_threshold);
        filter->expand(tmp_mask, tmp_bin, expand_radius);

        async_ops.push_back(tmp_contour->saveAsync("template_contour.png"));
        async_ops.push_back(tmp_bin->saveAsync("template_binary.png"));
        async_ops.push_back(tmp_mask->saveAsync("template_mask.png"));

        testPrint("Total (ref): %f\n", Total_Reference(tmp_contour).valf);
        testPrint("Total  (cs): %f\n", filter->total(tmp_contour).get().valf);

        tmp_size = tmp_gray->getSize();
        tmp_bits = filter->countBits(tmp_mask).get();
        testPrint("CountBits (ref): %d\n", CountBits_Reference(tmp_mask));
        testPrint("CountBits  (cs): %d\n", tmp_bits);
        testPrint("\n");
    }

    auto surface = gfx->createTextureFromFile("Screen.png");
    testExpect(surface != nullptr);

    {
        std::lock_guard<mr::IGfxInterface> lock(*gfx);

        //auto surf_gray, surf_cont, surf_binary, match;

        int2 size = int2(float2(surface->getSize()) * scale);
        auto surf_gray = gfx->createTexture(size.x, size.y, mr::TextureFormat::Ru8);
        auto surf_cont = gfx->createTexture(size.x, size.y, mr::TextureFormat::Ru8);
        auto surf_bin = gfx->createTexture(size.x, size.y, mr::TextureFormat::Binary);
        auto match = gfx->createTexture(size.x, size.y, mr::TextureFormat::Ri32);
        auto match_normalized = gfx->createTexture(size.x, size.y, mr::TextureFormat::Rf32);

        auto time_begin = test::Now();

        filter->transform(surf_gray, surface, true);
        filter->contour(surf_cont, surf_gray, contour_radius);
        filter->binarize(surf_bin, surf_cont, binarize_threshold);

        int2 offset = { 0, 0 };
        int2 range = surf_gray->getSize() - offset - tmp_size;
        if (tmp_image) {
            filter->match(match, surf_bin, tmp_bin, tmp_mask, {offset, range});
            filter->normalize(match_normalized, match, tmp_bits);
        }

        auto result = filter->minmax(match_normalized, range).get();

        auto elapsed = test::Now() - time_begin;
        testPrint("elapsed: %.2f ms\n", test::NS2MS(elapsed));
        testPrint("\n");

        if (match_normalized) {
            auto print = [&](auto r) {
                testPrint("  Min: %f (%d, %d)\n", r.valf_min, r.pos_min.x, r.pos_min.y);
                testPrint("  Max: %f (%d, %d)\n", r.valf_max, r.pos_max.x, r.pos_max.y);
            };

            testPrint("MinMax (ref):\n");
            print(MinMax_Reference(match_normalized, range));

            testPrint("MinMax  (cs):\n");
            print(result);

            auto marked = gfx->createTexture(surface->getSize().x, surface->getSize().y, mr::TextureFormat::RGBAu8);
            filter->copy(marked, surface);
            auto pos = int2(float2(result.pos_min + offset) / scale);
            auto size = int2(float2(tmp_size) / scale);
            DrawRect(marked, Rect{ pos, size }, 2, { 1.0f, 0.0f, 0.0f, 1.0f });

            //auto center = pos + size / 2;
            //auto radius = float(std::max(size.x, size.y)) / 2.0f;
            //DrawCircle(marked, center, radius, 2, { 1.0f, 0.0f, 0.0f, 1.0f });

            async_ops.push_back(marked->saveAsync("EntireScreen_match_result.png"));
        }

        if (surf_gray)
            async_ops.push_back(surf_gray->saveAsync("EntireScreen_grayscale.png"));
        if (surf_cont)
            async_ops.push_back(surf_cont->saveAsync("EntireScreen_contour.png"));
        if (surf_bin)
            async_ops.push_back(surf_bin->saveAsync("EntireScreen_binarize.png"));
        if (match_normalized)
            async_ops.push_back(match_normalized->saveAsync("EntireScreen_score.png"));
    }
    wait_async_ops();
}

testCase(ScalingFilter)
{
    static const float PI = 3.14159265359f;

    auto lanczos3 = [](float x)
    {
        if (x == 0.0f)
            return 1.0f;
        const float radius = 3.0f;
        float rx = x / radius;
        return (std::sin(PI * x) / (PI * x)) * (std::sin(PI * rx) / (PI * rx));
    };

    testPrint("lanczos3:\n");
    float2 offset{0.5f, 0.5f}; // -0.5 ~ 0.5
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 6; ++x) {
            float d = mr::length((float2{ (float)x, (float)y } - 2.5f + offset));
            float w = lanczos3(d);
            if (w < 0.0f)
                testPrint("%.2f ", w);
            else
                testPrint(" %.2f ", w);
        }
        testPrint("\n");
    }
    testPrint("\n");



    std::vector<std::future<bool>> async_ops;
    auto wait_async_ops = [&]() {
        for (auto& a : async_ops)
            a.wait();
        async_ops.clear();
    };

    ::Sleep(2000);
    mr::CaptureWindow(::GetForegroundWindow(), [](const void* data, int w, int h) {
        mr::SaveAsPNG("Window.png", w, h, mr::PixelFormat::BGRAu8, data, 0, true);
        });


    auto gfx = mr::GetGfxInterface();
    auto filter = mr::CreateFilterSet();
    auto surface = gfx->createTextureFromFile("Window.png");
    testExpect(surface != nullptr);

    auto resize_and_export = [&](int2 size, const char* path1, const char* path2) {
        auto with_filter = gfx->createTexture(size.x, size.y, mr::TextureFormat::RGBAu8);
        auto without_filter = gfx->createTexture(size.x, size.y, mr::TextureFormat::RGBAu8);

        filter->transform(with_filter, surface, false, true);
        filter->transform(without_filter, surface, false, false);

        async_ops.push_back(with_filter->saveAsync(path1));
        async_ops.push_back(without_filter->saveAsync(path2));
    };

    // downscale filter test
    resize_and_export(
        int2(float2(surface->getSize()) * 0.5f),
        "Window_downscale_with_filter.png",
        "Window_downscale_without_filter.png");

    // upscale filter test
    resize_and_export(
        int2(float2(surface->getSize()) * 3.0f),
        "Window_upscale_with_filter.png",
        "Window_upscale_without_filter.png");

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

    auto gfx = mr::GetGfxInterface();
    auto filter = mr::CreateFilterSet();

    auto time_start = mr::NowNS();
    auto scap = gfx->createScreenCapture();
    testExpect(scap != nullptr);

    mr::ITexture2DPtr surface;
    if (scap->startCapture(mr::GetPrimaryMonitor())) {
        for (int i = 0; i < 5; ++i) {
            auto frame = scap->waitNextFrame();
            if (!frame.surface)
                continue;

            testPrint("frame %d [%.2f ms]\n", i, float(double(frame.present_time - time_start) / 1000000.0));
            if (!surface)
                surface = gfx->createTexture(frame.size.x, frame.size.y, mr::TextureFormat::RGBAu8);
            filter->copy(surface, frame.surface);

            char filename[256];
            snprintf(filename, std::size(filename), "Frame%02d.png", i);
            async_ops.push_back(surface->saveAsync(filename));
        }
        scap->stopCapture();
    }
    wait_async_ops();
}



class Window
{
public:
    bool open(int2 size, const TCHAR* title);
    int2 getSize() const;
    void processMessages();
    void setPosition(int2 pos);

    // data: RGBAu8
    bool draw(Rect rect, const unorm8x4* src);
    bool draw(const unorm8x4* src);

private:
    static LRESULT CALLBACK msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd{};
    int2 m_size;
};


LRESULT CALLBACK Window::msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        ::PostQuitMessage(0);
        break;
    }
    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

int2 Window::getSize() const
{
    return m_size;
}

bool Window::open(int2 size, const TCHAR* title)
{
    m_size = size;

    const WCHAR* class_name = L"MarionetteTestWindow";
    DWORD style = WS_POPUPWINDOW;
    DWORD style_ex = WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED;

    WNDCLASS wc{};
    wc.lpfnWndProc = &msgProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.lpszClassName = class_name;

    if (::RegisterClass(&wc) != 0) {
        RECT r{ 0, 0, (LONG)size.x, (LONG)size.y };
        ::AdjustWindowRect(&r, style, false);
        int w = r.right - r.left;
        int h = r.bottom - r.top;
        m_hwnd = ::CreateWindowEx(style_ex, class_name, title, style, CW_USEDEFAULT, CW_USEDEFAULT, w, h, nullptr, nullptr, wc.hInstance, nullptr);
        if (m_hwnd) {
            //::SetLayeredWindowAttributes(m_hwnd, 0, 64, LWA_ALPHA);
            ::ShowWindow(m_hwnd, SW_SHOWNORMAL);
            return true;
        }
    }
    return false;
}

void Window::processMessages()
{
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
}

void Window::setPosition(int2 pos)
{
    RECT r{};
    ::GetWindowRect(m_hwnd, &r);
    ::SetWindowPos(m_hwnd, HWND_TOPMOST, pos.x, pos.y, (r.right - r.left), (r.bottom - r.top), SWP_NOSIZE | SWP_NOREDRAW);
}

bool Window::draw(Rect rect, const unorm8x4* src)
{
    HBITMAP hbmp{};
    {
        BITMAPV5HEADER bi{};
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = rect.size.x;
        bi.bV5Height = rect.size.y;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask = 0x000000FF;
        bi.bV5AlphaMask = 0xFF000000;

        uint32_t* dst{};
        HDC hdc = ::GetDC(m_hwnd);
        hbmp = ::CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&dst, nullptr, (DWORD)0);
        ::ReleaseDC(m_hwnd, hdc);
        if (hbmp == nullptr)
            return false;

        int l = rect.size.x * rect.size.y;
        for (int i = 0; i < l; ++i) {
            auto p = *src++;
            // RGBA -> ARGB
            *dst++ = (p.w.value << 24) | (p.x.value << 16) | (p.y.value << 8) | (p.z.value << 0);
        }
    }

    RECT  rc;
    ::GetWindowRect(m_hwnd, &rc);
    POINT screen_pos{ rc.left , rc.top };
    SIZE  size{ rect.size.x, rect.size.y };

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC hsdc = ::GetDC(nullptr);
    HDC hdc = ::GetDC(m_hwnd);
    HDC hmemdc = ::CreateCompatibleDC(hdc);

    POINT po{};
    HGDIOBJ hOldObj = ::SelectObject(hmemdc, hbmp);
    ::BitBlt(hdc, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, hmemdc, 0, 0, SRCCOPY | CAPTUREBLT);
    ::UpdateLayeredWindow(m_hwnd, hsdc, &screen_pos, &size, hmemdc, &po, 0, &blend, ULW_ALPHA);
    ::SelectObject(hmemdc, hOldObj);
    ::DeleteDC(hmemdc);
    ::ReleaseDC(m_hwnd, hdc);
    ::ReleaseDC(0, hsdc);

    ::DeleteObject(hbmp);
    return true;
}

bool Window::draw(const unorm8x4* src)
{
    return draw(Rect{ {}, m_size }, src);
}



testCase(ScreenMatcher)
{
    mr::IScreenMatcher::Params sm_params;
    sm_params.contour_radius = 1.5f;
    sm_params.expand_radius = 1.5f;
    auto matcher = mr::CreateScreenMatcher(sm_params);
    testExpect(matcher != nullptr);

    auto tmpl = matcher->createTemplate("template.png");
    testExpect(tmpl != nullptr);
    //tmpl->setMatchPattern(mr::ITemplate::MatchPattern::Grayscale);

    auto tsize = tmpl->getImage()->getSize();

    Window window;
    window.open(tsize, L"Marionette Tracking");
    {
        std::vector<unorm8x4> pixels(tsize.x * tsize.y);
        auto* dst = pixels.data();
        int bw = 2;
        unorm8x4 border_color = { 1.0f, 0.0f, 0.0f, 1.0f };
        unorm8x4 bg_color = { 0.0f, 0.0f, 0.0f, 0.0f };
        for (int y = 0; y < tsize.y; ++y) {
            for (int x = 0; x < tsize.x; ++x) {
                if (y < bw || y >= (tsize.y - bw) ||
                    x < bw || x >= (tsize.x - bw))
                {
                    *dst++ = border_color;
                }
                else {
                    *dst++ = bg_color;
                }
            }
        }
        window.draw(pixels.data());
    }

    ::Sleep(2000);

    auto get_target = []() {
        //return mr::GetPrimaryMonitor();
        return ::GetForegroundWindow();
    };
    mr::IScreenMatcher::Result last_result;

    auto time_start = mr::NowNS();
    for (int i = 0; i < 300; ++i) {
        auto time_begin_match = mr::NowNS();
        auto result = matcher->match(tmpl, get_target());
        auto time_end_match = mr::NowNS();

        auto elapsed = float(double(time_end_match - time_begin_match) / 1000000.0);
        auto t = float(double(time_end_match - time_start) / 1000000.0);
        auto pos = result.region.pos;
        testPrint("frame %d (%.1f): score %.4f (%d, %d) [%.2f ms]\n", i, t, result.score, pos.x, pos.y, elapsed);
        last_result = result;

        window.setPosition(pos);
        window.processMessages();

        mr::WaitVSync();
    }

#ifdef mrDebug
    if (last_result.result) {
        //mr::CreateFilterSet()->normalize(last_result.result, float(tsize.x * tsize.y))->save("score.png");
    }
#endif
}
