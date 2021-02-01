#pragma once
#include "mrGfxFoundation.h"

#define mrWithDesktopDuplicationAPI
#define mrWithWindowsGraphicsCapture

namespace mr {

class ScreenCaptureCommon : public RefCount<IScreenCapture>
{
public:
    FrameInfo getFrame() override;
    FrameInfo waitNextFrame() override;
    void setOnFrameArrived(const Callback& cb) override;

protected:
    void updateFrame(com_ptr<ID3D11Texture2D>& surface, int2 size, nanosec time);

protected:
    Callback m_callback;

    FrameInfo m_frame_info;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_waiting_next_frame{ false };

    Texture2DPtr m_prev_surface;
};


#ifdef mrWithDesktopDuplicationAPI

IScreenCapture* CreateDesktopDuplication_();
mrDefShared(CreateDesktopDuplication);

#endif // mrWithDesktopDuplicationAPI


#ifdef mrWithWindowsGraphicsCapture

bool IsWindowsGraphicsCaptureSupported();
IScreenCapture* CreateGraphicsCapture_();
mrDefShared(CreateGraphicsCapture);

#endif // mrWithWindowsGraphicsCapture

} // namespace mr
