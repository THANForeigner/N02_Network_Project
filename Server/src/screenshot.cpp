#include "screenshot.h"

void takeScreenShot()
{
    HWND hwnd = GetDesktopWindow();
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    HDC hwindowDC, hwindowCompatibleDC;
    HBITMAP hbwindow;
    cv::Mat src;
    BITMAPINFOHEADER bi;
    hwindowDC = GetDC(hwnd);
    hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
    src.create(height, width, CV_8UC4);
    hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
    SelectObject(hwindowCompatibleDC, hbwindow);
    BitBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, x, y, SRCCOPY);
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;
    GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);
    ReleaseDC(hwnd, hwindowDC);

    std::string savePath = "../data/screenshot/screenshot.png";
    cv::imwrite(savePath, src);
}