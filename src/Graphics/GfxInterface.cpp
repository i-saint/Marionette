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

    ITexture2DPtr createTexture(int w, int h, TextureFormat f, const void* data = nullptr, int pitch = 0) override;
    IScreenCapturePtr createScreenCapture() override;

    void transform(TransformParams& v) override;
    void contour(ContourParams& v) override;
    void match(MatchParams& v) override;
    void reduceMinMax(ReduceMinmaxParams& v) override;

    void flush() override;
    void wait() override;

    void lock() override;
    void unlock() override;

private:
    // filters
    Transform m_transform;
    Contour m_contour;
    TemplateMatch m_template_match;
    ReduceMinMax m_reduce_minmax;
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

IScreenCapturePtr GfxInterface::createScreenCapture()
{
    return CreateGraphicsCaptureShared();
}

inline std::shared_ptr<Texture2D>& i2c(std::shared_ptr<ITexture2D>& c) { return std::static_pointer_cast<Texture2D>(c); }

void GfxInterface::transform(TransformParams& v)
{
    if (!v.src)
        return;
    if (!v.dst) {
        auto size = v.src->getSize();
        if (v.scale != 1.0f)
            size = int2(float2(size) * v.scale);
        v.dst = Texture2D::create(size.x, size.y, v.src->getFormat());
    }

    m_transform.setSrcImage(i2c(v.src));
    m_transform.setDstImage(i2c(v.dst));
    m_transform.setCopyRegion(v.offset, v.size);
    m_transform.setFlipRB(v.flip_rb);
    m_transform.setGrayscale(v.grayscale);
    m_transform.dispatch();
}

void GfxInterface::contour(ContourParams& v)
{
    // todo
}

void GfxInterface::match(MatchParams& v)
{
    // todo
}

void GfxInterface::reduceMinMax(ReduceMinmaxParams& v)
{
    // todo
}

void GfxInterface::flush()
{
    mrGfxGlobals()->flush();
}

void GfxInterface::wait()
{
    mrGfxGlobals()->wait();
}

void GfxInterface::lock()
{
    mrGfxGlobals()->lock();
}

void GfxInterface::unlock()
{
    mrGfxGlobals()->unlock();
}

} // namespace mr
