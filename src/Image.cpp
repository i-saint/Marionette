#include "pch.h"
#include "MouseReplayer.h"

#ifdef mrWithOpenCV
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"shcore.lib")
#pragma comment(lib,"opencv_core451.lib")
#pragma comment(lib,"opencv_imgcodecs451.lib")
#pragma comment(lib,"opencv_imgproc451.lib")
#pragma comment(lib,"opencv_highgui451.lib")

namespace mr {

cv::Mat MakeCVImage(const void* data_, int width, int height, int src_pitch, bool flip_y)
{
    cv::Mat ret(height, width, CV_8UC4);
    auto data = (byte*)data_;

    int dst_pitch = width * 4;
    if (flip_y) {
        for (int i = 0; i < height; ++i) {
            auto* src = &data[src_pitch * (height - i - 1)];
            auto* dst = ret.ptr() + (dst_pitch * i);
            memcpy(dst, src, dst_pitch);
        }
    }
    else {
        for (int i = 0; i < height; ++i) {
            auto* src = &data[src_pitch * i];
            auto* dst = ret.ptr() + (dst_pitch * i);
            memcpy(dst, src, dst_pitch);
        }
    }
    return ret;
}

// Blt: [](HDC hscreen, HDC hdc) -> void
template<class Blt>
static cv::Mat CaptureImpl(RECT rect, HWND hwnd, const Blt& blt)
{
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    info.bmiHeader.biSizeImage = width * height * 4;
    info.bmiHeader.biClrUsed = 0;
    info.bmiHeader.biClrImportant = 0;


    HDC hscreen = ::GetDC(hwnd);
    HDC hdc = ::CreateCompatibleDC(hscreen);
    byte* data = nullptr;
    cv::Mat ret;
    if (HBITMAP hbmp = ::CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void**)(&data), NULL, NULL)) {
        ::SelectObject(hdc, hbmp);
        blt(hscreen, hdc);
        ret = MakeCVImage(data, width, height, width * 4, true);

        ::DeleteObject(hbmp);
    }
    ::DeleteDC(hdc);
    ::ReleaseDC(hwnd, hscreen);
    return ret;
}


cv::Mat CaptureScreen(RECT rect)
{
    int x = rect.left;
    int y = rect.top;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    mrProfile("CaptureScreen %dx%d at %d,%d: ", width, height, x, y);

    return CaptureImpl(rect, nullptr, [&](HDC hscreen, HDC hdc) {
        ::StretchBlt(hdc, 0, 0, width, height, hscreen, x, y, width, height, SRCCOPY);
        });
}

cv::Mat CaptureEntireScreen()
{
    int x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    mrProfile("CaptureEntireScreen %dx%d at %d,%d: ", width, height, x, y);

    return CaptureScreen({ x, y, width + x, height + y });
}

cv::Mat CaptureWindow(HWND hwnd)
{
    RECT rect{};
    ::GetWindowRect(hwnd, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    mrProfile("CaptureWindow %d %d: ", width, height);

    return CaptureImpl(rect, hwnd, [&](HDC hscreen, HDC hdc) {
        // BitBlt() can't capture Chrome, Edge, etc. PrintWindow() with PW_RENDERFULLCONTENT can do it.
        //::BitBlt(hdc, 0, 0, width, height, hscreen, 0, 0, SRCCOPY);
        ::PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT);
        });
}

struct ScreenData
{
    int index = 0;

    RECT screen_rect{};
    float scale_factor = 1.0f;
    cv::Mat image;

    cv::Point position{};
    double score = 0.0;

    std::future<void> task;

    void match(const MatchImageParams& args);
};
using ScreenDataPtr = std::shared_ptr<ScreenData>;

struct MatchImageCtx
{
    // inputs
    const MatchImageParams* args = nullptr;

    // outputs
    int screen_index = 0;
    std::vector<ScreenDataPtr> screens;
};

void ScreenData::match(const MatchImageParams& args)
{
//#define mrDbgScreenshots

    try {
#ifdef mrDbgScreenshots
        char file_screenshot[128];
        char file_result[128];
        char file_score[128];
        char file_binary[128];
        sprintf(file_screenshot, "screenshot%d.png", index);
        sprintf(file_result, "result%d.png", index);
        sprintf(file_score, "score%d.exr", index);
        sprintf(file_binary, "binary%d.png", index);
#endif // mrDbgScreenshots

        // resize images to half for faster matching

        float scale_half = 0.5f;
        float scale_screen = 0.5f;
        if (args.care_scale_factor)
            scale_screen /= scale_factor;

        if (args.target_window)
            image = CaptureWindow(args.target_window);
        else
            image = CaptureScreen(screen_rect);

        // resize images to half and convert to binary
#ifdef mrDbgScreenshots
        cv::imwrite(file_screenshot, image);
#endif // mrDbgScreenshots
        cv::resize(image, image, {}, scale_screen, scale_screen, cv::INTER_AREA);
        cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
        cv::adaptiveThreshold(image, image, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, args.block_size, args.color_offset);
#ifdef mrDbgScreenshots
        cv::imwrite(file_binary, image);
#endif // mrDbgScreenshots

        cv::Rect match_rect;
        for (auto& ti : args.template_images) {
            cv::Mat tmp_img;
            cv::resize(ti, tmp_img, {}, scale_half, scale_half, cv::INTER_AREA);
            cv::adaptiveThreshold(tmp_img, tmp_img, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, args.block_size, args.color_offset);

            // match!
            cv::Mat dst_img;
            cv::Point tpos;
            double tscore;
            cv::matchTemplate(image, tmp_img, dst_img, cv::TM_SQDIFF_NORMED);
            cv::minMaxLoc(dst_img, &tscore, nullptr, &tpos, nullptr);
            tscore = 1.0 - tscore;

            if (tscore > score) {
                score = tscore;
                match_rect = cv::Rect(tpos.x, tpos.y, tmp_img.cols, tmp_img.rows);

                // half-sized screen position to actual screen position
                position.x = (tpos.x + (tmp_img.cols / 2)) / scale_screen;
                position.y = (tpos.y + (tmp_img.rows / 2)) / scale_screen;
            }
        }

#ifdef mrDbgScreenshots
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
        cv::rectangle(image, match_rect, CV_RGB(255, 0, 0), 2);
        cv::imwrite(file_result, image);
        cv::imwrite(file_score, dst_img);
#endif // mrDbgScreenshots
    }
    catch (const cv::Exception& e) {
        DbgPrint("*** MatchImage() raised exception: %s ***\n", e.what());
    }
}


static BOOL MatchImageCB(HMONITOR hmon, HDC hdc, LPRECT rect, LPARAM userdata)
{
    auto ctx = (MatchImageCtx*)userdata;

    UINT dpix, dpiy;
    ::GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

    auto sdata = std::make_shared<ScreenData>();
    sdata->index = ctx->screen_index++;
    sdata->screen_rect = *rect;
    sdata->scale_factor = dpix / 96.0;
    ctx->screens.push_back(sdata);

    sdata->task = std::async(std::launch::async, [ctx, sdata]() {
        sdata->match(*ctx->args);
        });
    return TRUE;
}

static bool MatchImageWindow(MatchImageCtx* ctx)
{
    HWND hwnd = ctx->args->target_window;
    HMONITOR hmon = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (!hwnd || !hmon)
        return false;

    UINT dpix, dpiy;
    ::GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

    auto sdata = std::make_shared<ScreenData>();
    ::GetWindowRect(hwnd, &sdata->screen_rect);
    sdata->index = ctx->screen_index++;
    sdata->scale_factor = dpix / 96.0;
    ctx->screens.push_back(sdata);

    sdata->task = std::async(std::launch::async, [ctx, sdata]() {
        sdata->match(*ctx->args);
        });
    return true;
}

float MatchImage(MatchImageParams& args)
{
    mrProfile("MatchImage(): ");

    MatchImageCtx ctx;
    ctx.args = &args;

    if (args.match_target == MatchTarget::EntireScreen) {
        ::EnumDisplayMonitors(nullptr, nullptr, MatchImageCB, (LPARAM)&ctx);
    }
    else if (args.match_target == MatchTarget::ForegroundWindow) {
        auto fgw = ::GetForegroundWindow();
        if (!fgw || fgw == ::GetDesktopWindow() || fgw == GetShellWindow()) {
            // foreground window is desktop. capture entire screen.
            ::EnumDisplayMonitors(nullptr, nullptr, MatchImageCB, (LPARAM)&ctx);
        }
        else {
            args.target_window = ::GetTopWindow(fgw);
            MatchImageWindow(&ctx);
        }
    }

    ScreenDataPtr sdata;
    double highest_score = 0.0;
    // pick the screen with highest score
    for (auto& s : ctx.screens) {
        s->task.get();
        if (s->score > highest_score) {
            highest_score = s->score;
            sdata = s;
        }
    }

    DbgPrint("match score: %lf\n", highest_score);
    if (sdata) {
        args.score = highest_score;
        args.position = {
            sdata->position.x + sdata->screen_rect.left,
            sdata->position.y + sdata->screen_rect.top
        };
    }
    return highest_score;
}

} // namespace mr
#endif // mrWithOpenCV
