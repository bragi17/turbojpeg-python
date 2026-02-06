#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "turbojpeg_decoder.h"
#include <stdexcept>
#include <vector>
#include <memory>

namespace py = pybind11;

// 线程安全的解码器池
class DecoderPool {
public:
    DecoderPool(int pool_size = 4) : pool_size_(pool_size) {
        decoders_.reserve(pool_size);
        for (int i = 0; i < pool_size; ++i) {
            auto decoder = std::make_unique<TurboJpegDecoder>();
            if (!decoder->init()) {
                throw std::runtime_error("Failed to initialize decoder in pool");
            }
            decoders_.push_back(std::move(decoder));
        }
    }

    TurboJpegDecoder* acquire() {
        // 简单的轮询策略
        int idx = current_index_.fetch_add(1) % pool_size_;
        return decoders_[idx].get();
    }

private:
    std::vector<std::unique_ptr<TurboJpegDecoder>> decoders_;
    std::atomic<int> current_index_{0};
    int pool_size_;
};

class TurboJpegDecoderWrapper {
public:
    TurboJpegDecoderWrapper() {
        if (!decoder_.init()) {
            throw std::runtime_error("Failed to initialize TurboJPEG decoder");
        }
    }

    // 方法1: 标准解码（有拷贝）
    py::array_t<uint8_t> decode(const std::string& filename) {
        std::vector<uint8_t> data;
        int width, height, channels;

        if (!decoder_.decode(filename, data, width, height, channels)) {
            throw std::runtime_error("Failed to decode image: " + filename);
        }

        if (channels == 1) {
            return py::array_t<uint8_t>(
                py::buffer_info(
                    data.data(), sizeof(uint8_t),
                    py::format_descriptor<uint8_t>::format(),
                    2, { height, width },
                    { width * sizeof(uint8_t), sizeof(uint8_t) }
                )
            );
        } else {
            return py::array_t<uint8_t>(
                py::buffer_info(
                    data.data(), sizeof(uint8_t),
                    py::format_descriptor<uint8_t>::format(),
                    3, { height, width, channels },
                    { width * channels * sizeof(uint8_t),
                      channels * sizeof(uint8_t),
                      sizeof(uint8_t) }
                )
            );
        }
    }

    // 方法2: 获取图像信息
    py::tuple get_image_info(const std::string& filename) {
        int width, height, channels;
        if (!decoder_.get_image_info(filename, width, height, channels)) {
            throw std::runtime_error("Failed to get image info: " + filename);
        }
        return py::make_tuple(width, height, channels);
    }

    // 方法3: 零拷贝解码到预分配 buffer
    void decode_to_buffer(const std::string& filename,
                          py::array_t<uint8_t, py::array::c_style | py::array::forcecast> output_buffer) {
        py::buffer_info buf = output_buffer.request();
        if (buf.ndim != 2 && buf.ndim != 3) {
            throw std::runtime_error("Output buffer must be 2D or 3D array");
        }

        int width, height, channels;
        uint8_t* data_ptr = static_cast<uint8_t*>(buf.ptr);
        size_t buffer_size = buf.size * sizeof(uint8_t);

        if (!decoder_.decode_to_buffer(filename, data_ptr, buffer_size, width, height, channels)) {
            throw std::runtime_error("Failed to decode image: " + filename);
        }
    }

    // 方法4: 使用快速 DCT（牺牲一点质量换速度）
    py::array_t<uint8_t> decode_fast(const std::string& filename) {
        std::vector<uint8_t> data;
        int width, height, channels;

        if (!decoder_.decode_fast(filename, data, width, height, channels)) {
            throw std::runtime_error("Failed to decode image: " + filename);
        }

        if (channels == 1) {
            return py::array_t<uint8_t>(
                py::buffer_info(
                    data.data(), sizeof(uint8_t),
                    py::format_descriptor<uint8_t>::format(),
                    2, { height, width },
                    { width * sizeof(uint8_t), sizeof(uint8_t) }
                )
            );
        } else {
            return py::array_t<uint8_t>(
                py::buffer_info(
                    data.data(), sizeof(uint8_t),
                    py::format_descriptor<uint8_t>::format(),
                    3, { height, width, channels },
                    { width * channels * sizeof(uint8_t),
                      channels * sizeof(uint8_t),
                      sizeof(uint8_t) }
                )
            );
        }
    }

private:
    TurboJpegDecoder decoder_;
};

PYBIND11_MODULE(_decoder, m) {
    m.doc() = "TurboJPEG JPEG decoder plugin (optimized with zero-copy and fast DCT)";

    py::class_<TurboJpegDecoderWrapper>(m, "TurboJpegDecoder")
        .def(py::init<>())
        .def("decode", &TurboJpegDecoderWrapper::decode,
             "Decode JPEG file to new numpy array (has copy)")
        .def("get_image_info", &TurboJpegDecoderWrapper::get_image_info,
             "Get image dimensions (width, height, channels)")
        .def("decode_to_buffer", &TurboJpegDecoderWrapper::decode_to_buffer,
             "Decode JPEG directly to pre-allocated numpy buffer (zero-copy)")
        .def("decode_fast", &TurboJpegDecoderWrapper::decode_fast,
             "Decode JPEG with fast DCT algorithm (slightly lower quality, faster)");
}
