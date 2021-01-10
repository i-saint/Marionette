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

cv::Mat CaptureScreenshot(RECT rect)
{
    int x = rect.left;
    int y = rect.top;
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

    cv::Mat ret(height, width, CV_8UC3);
    byte* data;

    HDC dc = ::GetDC(nullptr);
    HDC cdc = ::CreateCompatibleDC(dc);
    if (HBITMAP hbmp = ::CreateDIBSection(cdc, &info, DIB_RGB_COLORS, (void**)(&data), NULL, NULL)) {
        ::SelectObject(cdc, hbmp);
        ::StretchBlt(cdc, 0, 0, width, height, dc, x, y, width, height, SRCCOPY);

        auto* dst = ret.ptr();
        for (int y = 0; y < height; ++y) {
            auto* src = &data[4 * width * (height - y - 1)];
            for (int x = 0; x < width; ++x) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst += 3;
                src += 4;
            }
        }

        ::DeleteObject(hbmp);
    }
    ::DeleteDC(cdc);
    ::ReleaseDC(nullptr, dc);
    return ret;
}

cv::Mat CaptureScreenshot()
{
    int x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return CaptureScreenshot({ x, y, width + x, height + y });
}

struct ScreenData
{
    RECT screen_rect{};
    float scale_factor = 1.0f;
    cv::Mat image;

    cv::Point position{};
    double score = 0.0;
};
using ScreenDataPtr = std::shared_ptr<ScreenData>;

struct MatchImageCtx
{
    cv::Mat tmp_img;
    double highest_score = 0.0;
    std::vector<ScreenDataPtr> screens;
};

static BOOL MatchImageCB(HMONITOR hmon, HDC hdc, LPRECT rect, LPARAM userdata)
{
    auto ctx = (MatchImageCtx*)userdata;

    UINT dpix, dpiy;
    ::GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

    try {
        auto sdata = std::make_shared<ScreenData>();
        sdata->screen_rect = *rect;
        sdata->scale_factor = dpix / 96.0;
        sdata->image = CaptureScreenshot(*rect);
        //cv::imwrite("screenshot.png", image);

        // dps-aware scaling + resize to half
        double s = (1.0 / sdata->scale_factor) * 0.5;
        cv::resize(sdata->image, sdata->image, {}, s, s);
        cv::cvtColor(sdata->image, sdata->image, cv::COLOR_BGR2GRAY);

        cv::Mat dst_img;
        cv::matchTemplate(sdata->image, ctx->tmp_img, dst_img, cv::TM_CCORR_NORMED);
        cv::minMaxLoc(dst_img, nullptr, &sdata->score, nullptr, &sdata->position);

        //// debug output
        //cv::cvtColor(sdata->image, sdata->image, cv::COLOR_GRAY2BGR);
        //cv::rectangle(sdata->image, cv::Rect(sdata->position.x, sdata->position.y, ctx->tmp_img.cols, ctx->tmp_img.rows), CV_RGB(255, 0, 0), 2);
        //cv::imwrite("result.png", sdata->image);

        ctx->screens.push_back(sdata);
        ctx->highest_score = std::max(ctx->highest_score, sdata->score);
        sdata->position.x /= s;
        sdata->position.y /= s;
    }
    catch (const cv::Exception& e) {
        DbgPrint("*** MatchImage() raised exception: %s ***\n", e.what());
    }

    return TRUE;
}


std::tuple<bool, int, int> MatchImage(const cv::Mat& tmp_img, double threshold)
{
    MatchImageCtx ctx;
    cv::resize(tmp_img, ctx.tmp_img, {}, 0.5, 0.5);

    ::EnumDisplayMonitors(nullptr, nullptr, MatchImageCB, (LPARAM)&ctx);
    DbgPrint("match score: %lf\n", ctx.highest_score);

    if (ctx.highest_score >= threshold) {
        for (auto& sd : ctx.screens) {
            if (sd->score == ctx.highest_score) {
                return {
                    true,
                    sd->position.x + sd->screen_rect.left + (tmp_img.cols / 2),
                    sd->position.y + sd->screen_rect.top + (tmp_img.rows / 2)
                };
            }
        }
    }
    return { false, 0, 0 };
}

void TestCaptureScreenshot()
{
    ::Sleep(1000);

    RECT rect{};
    ::GetWindowRect(::GetForegroundWindow(), &rect);

    auto mat = CaptureScreenshot(rect);
    cv::imwrite("out.png", mat);
}

void TestMatchTemplate()
{
    cv::Mat src_img = cv::imread("src.png", cv::IMREAD_GRAYSCALE);
    cv::Mat tmp_img = cv::imread("template.png", cv::IMREAD_GRAYSCALE);
    cv::resize(tmp_img, tmp_img, cv::Size(), 2, 2);

    int d1 = src_img.type();
    int d2 = tmp_img.type();

    cv::Mat dst_img;
    cv::matchTemplate(src_img, tmp_img, dst_img, cv::TM_CCOEFF_NORMED);

    double min_val, max_val;
    cv::Point pos1, pos2;
    cv::minMaxLoc(dst_img, &min_val, &max_val, &pos1, &pos2);

    cv::rectangle(src_img, cv::Rect(pos2.x, pos2.y, tmp_img.cols, tmp_img.rows), CV_RGB(255, 0, 0), 2);
    cv::imwrite("result.png", src_img);
    cv::imwrite("match.exr", dst_img);
}

} // namespace mr
#endif // mrWithOpenCV
