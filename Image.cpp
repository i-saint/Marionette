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

// todo: try WindowsGraphicsCapture
// https://blogs.windows.com/windowsdeveloper/2019/09/16/new-ways-to-do-screen-capture/

cv::Mat CaptureScreenshot(RECT rect)
{
    int x = rect.left;
    int y = rect.top;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    DbgProfile("CaptureScreenshot %d %d: ", width, height);

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

    HDC hscreen = ::GetDC(nullptr);
    HDC hdc = ::CreateCompatibleDC(hscreen);
    if (HBITMAP hbmp = ::CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void**)(&data), NULL, NULL)) {
        ::SelectObject(hdc, hbmp);
        ::StretchBlt(hdc, 0, 0, width, height, hscreen, x, y, width, height, SRCCOPY);

        auto* dst = ret.ptr();
        for (int i = 0; i < height; ++i) {
            auto* src = &data[4 * width * (height - i - 1)];
            for (int j = 0; j < width; ++j) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst += 3;
                src += 4;
            }
        }

        ::DeleteObject(hbmp);
    }
    ::DeleteDC(hdc);
    ::ReleaseDC(nullptr, hscreen);
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

        float scale = 0.5f;
        if (args.care_scale_factor)
            scale /= scale_factor;

        image = CaptureScreenshot(screen_rect);
        cv::resize(image, image, {}, scale, scale, cv::INTER_AREA);
#ifdef mrDbgScreenshots
        cv::imwrite(file_screenshot, image);
#endif // mrDbgScreenshots

        // to gray scale
        cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);

        int blocksize = args.block_size;
        double C = args.color_offset;
        cv::adaptiveThreshold(image, image, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, blocksize, C);
#ifdef mrDbgScreenshots
        cv::imwrite(file_binary, image);
#endif // mrDbgScreenshots

        cv::Mat tmp_img;
        cv::resize(*args.tmplate_imgage, tmp_img, {}, 0.5, 0.5, cv::INTER_AREA);
        cv::adaptiveThreshold(tmp_img, tmp_img, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, blocksize, C);

        cv::Mat dst_img;
        cv::Point match_pos;

        //// CCOEFF
        //cv::matchTemplate(image, tmp_img, dst_img, cv::TM_CCOEFF_NORMED);
        //cv::minMaxLoc(dst_img, nullptr, &score, nullptr, &match_pos);

        // SQDIFF
        cv::matchTemplate(image, tmp_img, dst_img, cv::TM_SQDIFF_NORMED);
        cv::minMaxLoc(dst_img, &score, nullptr, &match_pos, nullptr);
        score = 1.0 - score;

        // cuda + SQDIFF
        //cv::cuda::getCudaEnabledDeviceCount();
        //cv::cuda::GpuMat gsrc(image);
        //cv::cuda::GpuMat gtmp(tmp_img);
        //cv::cuda::GpuMat gdst(tmp_img);
        //cv::matchTemplate(gsrc, gtmp, gdst, cv::TM_SQDIFF_NORMED);
        //cv::minMaxLoc(dst_img, &score, nullptr, &match_pos, nullptr);
        //score = 1.0 - score;

        // half-sized screen position to actual screen position
        position.x = (match_pos.x + (tmp_img.cols / 2)) / scale;
        position.y = (match_pos.y + (tmp_img.rows / 2)) / scale;

#ifdef mrDbgScreenshots
        cv::cvtColor(image, image, cv::COLOR_GRAY2BGR);
        cv::rectangle(image, cv::Rect(match_pos.x, match_pos.y, tmp_img.cols, tmp_img.rows), CV_RGB(255, 0, 0), 2);
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


float MatchImage(MatchImageParams& args)
{
    DbgProfile("MatchImage(): ");

    MatchImageCtx ctx;
    ctx.args = &args;

    ::EnumDisplayMonitors(nullptr, nullptr, MatchImageCB, (LPARAM)&ctx);

    ScreenDataPtr sdata;
    double highest_score = 0.0;
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
