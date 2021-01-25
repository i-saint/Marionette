#include "pch.h"
#include "Internal.h"
#include "Filter.h"
#include "ScreenCapture.h"

namespace mr {

class GfxInterface : public IGfxInterface
{
public:
    GfxInterface();
    ~GfxInterface() override;
    void release() override;

    ITexture2DPtr createTexture(int w, int h, TextureFormat f, const void* data, int pitch) override;
    ITexture2DPtr createTextureFromFile(const char* path) override;
    IScreenCapturePtr createScreenCapture() override;

    ITransformPtr createTransform() override;
    IBinarizePtr createBinarize() override;
    IContourPtr createContour() override;
    ITemplateMatchPtr createTemplateMatch() override;
    IReduceMinMaxPtr createReduceMinMax() override;

    void flush() override;
    void sync(int timeout_ms) override;

    void lock() override;
    void unlock() override;

private:
};


GfxInterface::GfxInterface()
{
}

GfxInterface::~GfxInterface()
{
}

void GfxInterface::release()
{
    delete this;
}

ITexture2DPtr GfxInterface::createTexture(int w, int h, TextureFormat f, const void* data, int pitch)
{
    return Texture2D::create(w, h, f, data, pitch);
}

ITexture2DPtr GfxInterface::createTextureFromFile(const char* path)
{
    return Texture2D::create(path);
}

IScreenCapturePtr GfxInterface::createScreenCapture()
{
    return CreateGraphicsCaptureShared();
}


ITransformPtr GfxInterface::createTransform()
{
    return mrGfxGetCS(TransformCS)->createContext();
}

IBinarizePtr GfxInterface::createBinarize()
{
    return mrGfxGetCS(BinarizeCS)->createContext();
}

IContourPtr GfxInterface::createContour()
{
    return mrGfxGetCS(ContourCS)->createContext();
}

ITemplateMatchPtr GfxInterface::createTemplateMatch()
{
    return mrGfxGetCS(TemplateMatchCS)->createContext();
}

IReduceMinMaxPtr GfxInterface::createReduceMinMax()
{
    return mrGfxGetCS(ReduceMinMaxCS)->createContext();
}


void GfxInterface::flush()
{
    mrGfxGlobals()->flush();
}

void GfxInterface::sync(int timeout_ms)
{
    mrGfxGlobals()->sync(timeout_ms);
}

void GfxInterface::lock()
{
    mrGfxGlobals()->lock();
}

void GfxInterface::unlock()
{
    mrGfxGlobals()->unlock();
}

mrAPI IGfxInterface* CreateGfxInterface()
{
    return new GfxInterface();
}

} // namespace mr
