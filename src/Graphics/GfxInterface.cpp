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

    ITexture2D* createTexture_(int w, int h, TextureFormat f, const void* data, int pitch) override;
    ITexture2D* createTextureFromFile_(const char* path) override;
    IScreenCapture* createScreenCapture_() override;

    ITransform* createTransform_() override;
    IBinarize* createBinarize_() override;
    IContour* createContour_() override;
    ITemplateMatch* createTemplateMatch_() override;
    IReduceMinMax* createReduceMinMax_() override;

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

ITexture2D* GfxInterface::createTexture_(int w, int h, TextureFormat f, const void* data, int pitch)
{
    return Texture2D::create_(w, h, f, data, pitch);
}

ITexture2D* GfxInterface::createTextureFromFile_(const char* path)
{
    return Texture2D::create_(path);
}

IScreenCapture* GfxInterface::createScreenCapture_()
{
    return CreateGraphicsCapture_();
}


ITransform* GfxInterface::createTransform_()
{
    return mrGfxGetCS(TransformCS)->createContext_();
}

IBinarize* GfxInterface::createBinarize_()
{
    return mrGfxGetCS(BinarizeCS)->createContext_();
}

IContour* GfxInterface::createContour_()
{
    return mrGfxGetCS(ContourCS)->createContext_();
}

ITemplateMatch* GfxInterface::createTemplateMatch_()
{
    return mrGfxGetCS(TemplateMatchCS)->createContext_();
}

IReduceMinMax* GfxInterface::createReduceMinMax_()
{
    return mrGfxGetCS(ReduceMinMaxCS)->createContext_();
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

mrAPI IGfxInterface* CreateGfxInterface_()
{
    return new GfxInterface();
}

} // namespace mr
