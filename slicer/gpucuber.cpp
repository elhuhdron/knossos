#include "gpucuber.h"

#include "segmentation/segmentation.h"

#include <boost/multi_array.hpp>

gpu_raw_cube::gpu_raw_cube(const int gpucubeedge, const bool index) {
    cube.setAutoMipMapGenerationEnabled(false);
    cube.setSize(gpucubeedge, gpucubeedge, gpucubeedge);
    cube.setMipLevels(1);
    cube.setMinificationFilter(index ? QOpenGLTexture::Nearest : QOpenGLTexture::Linear);
    cube.setMagnificationFilter(index ? QOpenGLTexture::Nearest : QOpenGLTexture::Linear);
    cube.setFormat(index ? QOpenGLTexture::R16_UNorm : QOpenGLTexture::R8_UNorm);
    cube.setWrapMode(QOpenGLTexture::ClampToEdge);
    cube.allocateStorage();
}

void gpu_raw_cube::generate(boost::multi_array_ref<uint8_t, 3>::const_array_view<3>::type view) {
    std::vector<char> data;
    for (const auto & d2 : view)
    for (const auto & d1 : d2)
    for (const auto & elem : d1) {
        data.emplace_back(elem);
    }
    cube.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data.data());
}

gpu_lut_cube::gpu_lut_cube(const int gpucubeedge) : gpu_raw_cube(gpucubeedge, true) {
    lut.setAutoMipMapGenerationEnabled(false);
    lut.setMipLevels(1);
    lut.setMinificationFilter(QOpenGLTexture::Nearest);
    lut.setMagnificationFilter(QOpenGLTexture::Nearest);
    lut.setFormat(QOpenGLTexture::RGBA8_UNorm);
}

void gpu_lut_cube::generate(boost::multi_array_ref<uint64_t, 3>::const_array_view<3>::type view) {
    std::vector<gpu_index> data;
    for (const auto & d2 : view)
    for (const auto & d1 : d2)
    for (const auto & elem : d1) {
        const auto it = id_to_lut_index.find(elem);
        const auto existing = it != std::end(id_to_lut_index);
        const auto index = existing ? it->second : (id_to_lut_index[elem] = highest_index++);//increment after assignment
        data.emplace_back(index);
        if (!existing) {
            const auto color = Segmentation::singleton().colorObjectFromSubobjectId(elem);
            colors.push_back({std::get<0>(color), std::get<1>(color), std::get<2>(color), std::get<3>(color)});
        }
    }
    const auto lutSize = std::pow(2, std::ceil(std::log2(colors.size())));
    colors.resize(lutSize);
    lut.setSize(lutSize);
    lut.allocateStorage();

    cube.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16, data.data());
    lut.setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt32_RGBA8_Rev, colors.data());
}

TextureLayer::TextureLayer(std::function<void()> getctx) : getctx{getctx} {}
TextureLayer::~TextureLayer() {
    getctx();//QOpenGLTexture dtor needs a current ctx
}

template<typename cube_type, typename elem_type>
void TextureLayer::createBogusCube(const int cpucubeedge, const int gpucubeedge) {
    getctx();
    std::vector<char> data;
    data.resize(std::pow(cpucubeedge, 3) * sizeof(elem_type));
    std::fill(std::begin(data), std::end(data), 0);
    bogusCube.reset(new cube_type(gpucubeedge));
    boost::multi_array_ref<elem_type, 3> cube(reinterpret_cast<elem_type*>(data.data()), boost::extents[cpucubeedge][cpucubeedge][cpucubeedge]);
    using range = boost::multi_array_types::index_range;
    static_cast<cube_type*>(bogusCube.get())->generate(cube[boost::indices[range(0,gpucubeedge)][range(cpucubeedge-gpucubeedge,cpucubeedge-0)][range(0,gpucubeedge)]]);
}

void TextureLayer::createBogusCube(const int cpucubeedge, const int gpucubeedge) {
    if (isOverlayData) {
        createBogusCube<gpu_lut_cube, std::uint64_t>(cpucubeedge, gpucubeedge);
    } else {
        createBogusCube<gpu_raw_cube, std::uint8_t>(cpucubeedge, gpucubeedge);
    }
}

template<typename cube_type, typename elem_type>
void TextureLayer::cubeAll(const boost::const_multi_array_ref<elem_type, 3> cube, const int cpucubeedge, const int gpucubeedge, const int x, const int y, const int z) {
    getctx();
    const auto factor = cpucubeedge / gpucubeedge;
    for (int zi = z * factor; zi < (z + 1) * factor; ++zi)
    for (int yi = y * factor; yi < (y + 1) * factor; ++yi)
    for (int xi = x * factor; xi < (x + 1) * factor; ++xi) {
        const auto x_offset = gpucubeedge * (xi % factor);
        const auto y_offset = gpucubeedge * (yi % factor);
        const auto z_offset = gpucubeedge * (zi % factor);
        using range = boost::multi_array_types::index_range;
        const auto view = cube[boost::indices[range(0+z_offset,gpucubeedge+z_offset)][range(0+y_offset,gpucubeedge+y_offset)][range(0+x_offset,gpucubeedge+x_offset)]];
        textures[QVector3D(xi, yi, zi)].reset(new cube_type(gpucubeedge));
        static_cast<cube_type*>(textures[QVector3D(xi, yi, zi)].get())->generate(view);
    }
}

void TextureLayer::cubeAll(const char * data, const int cpucubeedge, const int gpucubeedge, const int x, const int y, const int z) {
    if (isOverlayData) {
        boost::const_multi_array_ref<std::uint64_t, 3> cube(reinterpret_cast<const std::uint64_t *>(data), boost::extents[cpucubeedge][cpucubeedge][cpucubeedge]);
        cubeAll<gpu_lut_cube>(cube, cpucubeedge, gpucubeedge, x, y, z);
    } else {
        boost::const_multi_array_ref<std::uint8_t, 3> cube(reinterpret_cast<const std::uint8_t *>(data), boost::extents[cpucubeedge][cpucubeedge][cpucubeedge]);
        cubeAll<gpu_raw_cube>(cube, cpucubeedge, gpucubeedge, x, y, z);
    }
}
