#include "pch.h"
#include "Internal.h"
#include "Filter.h"

namespace mr {

class GfxInterface : public IGfxInterface
{
public:
    ~GfxInterface() override;
    void release() override;

    ITexture2DPtr createTexture(int w, int h, TextureFormat f, const void* data, int pitch) override;
    ITexture2DPtr captureScreen(HWND hwnd) override;
    ITexture2DPtr captureMonitor(HMONITOR hmon) override;

    void transform(TransformParams& v) override;
    void contour(ContourParams& v) override;
    void match(MatchParams& v) override;
    std::future<ReduceMinmaxResult> reduceMinMax(ReduceMinmaxParams& v) override;

    void flush() override;
    void wait() override;

private:
};

} // namespace mr
