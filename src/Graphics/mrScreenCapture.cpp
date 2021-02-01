#include "pch.h"
#include "mrInternal.h"
#include "mrScreenCapture.h"

namespace mr {

IScreenCapture::FrameInfo ScreenCaptureCommon::getFrame()
{
    FrameInfo ret;
    {
        std::unique_lock l(m_mutex);
        ret = m_frame_info;
    }
    return ret;
}

IScreenCapture::FrameInfo ScreenCaptureCommon::waitNextFrame()
{
    FrameInfo ret;
    {
        std::unique_lock l(m_mutex);
        m_waiting_next_frame = true;
        m_cond.wait(l, [this]() { return m_waiting_next_frame == false; });

        ret = m_frame_info;
    }
    return ret;
}

void ScreenCaptureCommon::setOnFrameArrived(const Callback& cb)
{
    std::unique_lock l(m_mutex);
    m_callback = cb;
}

void ScreenCaptureCommon::updateFrame(com_ptr<ID3D11Texture2D>& surface, int2 size, nanosec time)
{
    Texture2DPtr tex;
    if (m_prev_surface && surface.get() == m_prev_surface->ptr())
        tex = m_prev_surface;
    else
        tex = m_prev_surface = Texture2D::wrap(surface);

    if (size == int2::zero())
        size = tex->getSize();

    FrameInfo tmp{ tex, size, time };
    {
        std::unique_lock l(m_mutex);
        m_frame_info = tmp;

        if (m_callback)
            m_callback(m_frame_info);

        if (m_waiting_next_frame) {
            m_waiting_next_frame = false;
            m_cond.notify_one();
        }
    }
}

} // namespace mr
