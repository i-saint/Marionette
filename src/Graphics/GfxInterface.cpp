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

    void transform(TransformParams& v) override;
    void binarize(BinarizeParams& v) override;
    void contour(ContourParams& v) override;
    void templateMatch(TemplateMatchParams& v) override;
    void reduceMinMax(ReduceMinmaxParams& v) override;

    void flush() override;
    void sync(int timeout_ms) override;

    void lock() override;
    void unlock() override;

private:
    // filters
    Transform m_transform;
    Binarize m_binarize;
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

ITexture2DPtr GfxInterface::createTextureFromFile(const char* path)
{
    return Texture2D::create(path);
}

IScreenCapturePtr GfxInterface::createScreenCapture()
{
    return CreateGraphicsCaptureShared();
}


inline std::shared_ptr<Texture2D> i2c(std::shared_ptr<ITexture2D>& c)
{
    return std::static_pointer_cast<Texture2D>(c);
}

void GfxInterface::transform(TransformParams& v)
{
    if (!v.src)
        return;
    if (!v.dst) {
        auto size = v.src->getSize();
        if (v.scale != 1.0f)
            size = int2(float2(size) * v.scale);
        v.dst = Texture2D::create(size.x, size.y, v.grayscale ? TextureFormat::Ru8 : v.src->getFormat());
    }

    m_transform.setSrcImage(i2c(v.src));
    m_transform.setDstImage(i2c(v.dst));
    m_transform.setCopyRegion(v.offset, v.size);
    m_transform.setFlipRB(v.flip_rb);
    m_transform.setGrayscale(v.grayscale);
    m_transform.dispatch();
}

void GfxInterface::binarize(BinarizeParams& v)
{
    if (!v.src)
        return;
    if (!v.dst) {
        auto size = v.src->getSize();
        v.dst = Texture2D::create(ceildiv(size.x, 32), size.y, TextureFormat::Ri32);
    }

    m_binarize.setSrcImage(i2c(v.src));
    m_binarize.setDstImage(i2c(v.dst));
    m_binarize.setThreshold(v.threshold);
    m_binarize.dispatch();
}

void GfxInterface::contour(ContourParams& v)
{
    if (!v.src)
        return;
    if (!v.dst) {
        auto size = v.src->getSize();
        v.dst = Texture2D::create(size.x, size.y, TextureFormat::Ru8);
    }

    m_contour.setSrcImage(i2c(v.src));
    m_contour.setDstImage(i2c(v.dst));
    m_contour.setBlockSize(v.block_size);
    m_contour.dispatch();
}

void GfxInterface::templateMatch(TemplateMatchParams& v)
{
    if (!v.src || !v.template_image)
        return;
    if (v.src->getFormat() != v.template_image->getFormat()) {
        mrDbgPrint("*** GfxInterface::templateMatch(): format mismatch ***\n");
        return;
    }
    if (!v.dst) {
        if (v.src->getFormat() == TextureFormat::Ru8) {
            auto size = v.src->getSize() - v.template_image->getSize();
            v.dst = Texture2D::create(size.x, size.y, TextureFormat::Rf32);
        }
        else if (v.src->getFormat() == TextureFormat::Ri32) {
            auto size = v.src->getSize() - v.template_image->getSize();
            v.dst = Texture2D::create(size.x * 32, size.y, TextureFormat::Rf32);
        }
        if (!v.dst)
            return;
    }

    m_template_match.setSrcImage(i2c(v.src));
    m_template_match.setTemplateImage(i2c(v.template_image));
    m_template_match.setDstImage(i2c(v.dst));
    m_template_match.dispatch();
}

void GfxInterface::reduceMinMax(ReduceMinmaxParams& v)
{
    if (!v.src)
        return;

    m_reduce_minmax.setSrcImage(i2c(v.src));
    m_reduce_minmax.dispatch();
    v.result = m_reduce_minmax.getResult();
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
