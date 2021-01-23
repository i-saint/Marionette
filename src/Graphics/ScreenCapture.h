#pragma once
#include "GfxFoundation.h"

namespace mr {

#define mrWithDesktopDuplicationAPI
#define mrWithWindowsGraphicsCapture


#ifdef mrWithDesktopDuplicationAPI

IScreenCapture* CreateDesktopDuplication();
mrDefShared(CreateDesktopDuplication);

#endif // mrWithDesktopDuplicationAPI


#ifdef mrWithWindowsGraphicsCapture

bool IsWindowsGraphicsCaptureSupported();
IScreenCapture* CreateGraphicsCapture();
mrDefShared(CreateGraphicsCapture);

#endif // mrWithWindowsGraphicsCapture

} // namespace mr
