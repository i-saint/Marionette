#include "pch.h"
#include "Internal.h"
#include "Shader.h"
#include "ScreenCapture.h"

namespace mr {

class GfxInterface : public RefCount<IGfxInterface>
{
public:
    ITexture2DPtr createTexture(int w, int h, TextureFormat f, const void* data, int pitch) override;
    ITexture2DPtr createTextureFromFile(const char* path) override;
    IScreenCapturePtr createScreenCapture() override;

#define Body(Name) I##Name##Ptr create##Name() override;
mrEachCS(Body)
#undef Body

    void flush() override;
    void sync(int timeout_ms) override;

    void lock() override;
    void unlock() override;

private:
};


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
    return CreateGraphicsCapture();
}

#define Body(Name) I##Name##Ptr GfxInterface::create##Name() { return mrGfxGetCS(Name##CS)->createContext(); }
mrEachCS(Body)
#undef Body


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
