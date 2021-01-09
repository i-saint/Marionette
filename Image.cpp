#include "pch.h"

#ifdef mrWithOpenCV
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"opencv_core451.lib")
#pragma comment(lib,"opencv_imgcodecs451.lib")
#pragma comment(lib,"opencv_imgproc451.lib")
#pragma comment(lib,"opencv_highgui451.lib")

namespace mr {

cv::Mat CaptureScreenshot(HWND wnd)
{
    RECT rect;
    ::GetWindowRect(wnd, &rect);

    HDC dc = ::GetDC(wnd);
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
    ::ReleaseDC(wnd, dc);

    return ret;
 
    //auto monitor = ::MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
}

void TestCaptureScreenshot()
{
    ::Sleep(3000);
    auto mat = CaptureScreenshot(::GetForegroundWindow());
    cv::imwrite("out.png", mat);
}

} // namespace mr
#endif // mrWithOpenCV
