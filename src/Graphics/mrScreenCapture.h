#pragma once
#include "mrGfxFoundation.h"

namespace mr {

#define mrWithDesktopDuplicationAPI
#define mrWithWindowsGraphicsCapture


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
