#include "pch.h"
#include "mrInternal.h"
#include "mrShader.h"
#include "mrScreenCapture.h"

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
    auto ret = CreateGraphicsCapture();
    if (!ret)
        ret = CreateDesktopDuplication();
    return ret;

    //return CreateDesktopDuplication();
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


static IGfxInterfacePtr g_gfx_ifs;

mrAPI IGfxInterface* GetGfxInterface_()
{
    if (!g_gfx_ifs) {
        g_gfx_ifs = make_ref<GfxInterface>();
        AddFinalizeHandler([]() { g_gfx_ifs = nullptr; });
    }
    return g_gfx_ifs;
}

} // namespace mr
