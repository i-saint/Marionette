#include "pch.h"
#include "MouseReplayer.h"

#ifdef mrWithOpenCV
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"opencv_core451.lib")
#pragma comment(lib,"opencv_imgcodecs451.lib")
#pragma comment(lib,"opencv_imgproc451.lib")
#pragma comment(lib,"opencv_highgui451.lib")

namespace mr {

cv::Mat CaptureScreenshot(HWND hwnd)
{
    RECT rect{};
    if (!hwnd) {
        rect.right = ::GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = ::GetSystemMetrics(SM_CYSCREEN);
    }
    else {
        ::GetWindowRect(hwnd, &rect);
    }

    HDC dc = ::GetDC(hwnd);
    HDC hdc = ::CreateCompatibleDC(dc);
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
    if (HBITMAP hbmp = ::CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void**)(&data), NULL, NULL)) {
        ::SelectObject(hdc, hbmp);
        ::BitBlt(hdc, 0, 0, width, height, dc, 0, 0, SRCCOPY);

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
    ::DeleteDC(hdc);
    ::ReleaseDC(hwnd, dc);

    return ret;
 
    //auto monitor = ::MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
}

std::tuple<bool, int, int> MatchImage(HWND hwnd, const cv::Mat& tmp_img)
{
    try {
        cv::Mat screenshot;
        cv::cvtColor(CaptureScreenshot(hwnd), screenshot, cv::COLOR_BGR2GRAY);
        //cv::cvtColor(tmp_img, tmp_img, cv::IMREAD_GRAYSCALE);

        cv::Mat dst_img;
        cv::matchTemplate(screenshot, tmp_img, dst_img, cv::TM_CCORR_NORMED);

        double min_val, max_val;
        cv::Point pos1, pos2;
        cv::minMaxLoc(dst_img, &min_val, &max_val, &pos1, &pos2);

        if (max_val >= 0.9) {
            return {
                true,
                pos2.x + (tmp_img.cols / 2),
                pos2.y + (tmp_img.rows / 2)
            };
        }
    }
    catch (const cv::Exception& e) {
        DbgPrint("*** MatchImage() raises exxception: %s ***\n", e.what());
    }
    return { false, 0, 0 };
}

void TestCaptureScreenshot()
{
    ::Sleep(1000);
    //auto mat = CaptureScreenshot(::GetForegroundWindow());
    auto mat = CaptureScreenshot(nullptr);
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
