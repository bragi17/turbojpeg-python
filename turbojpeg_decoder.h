#ifndef TURBOJPEG_DECODER_H
#define TURBOJPEG_DECODER_H

#include <cstdint>
#include <string>
#include <vector>

// Forward declaration for TurboJPEG handle
typedef void* tjhandle;

class TurboJpegDecoder {
public:
    TurboJpegDecoder();
    ~TurboJpegDecoder();

    // Initialize decoder
    bool init();

    // Decode JPEG file to BGR format (OpenCV compatible)
    // Output format: HWC, BGR, uint8
    bool decode(const std::string& filename,
                std::vector<uint8_t>& output,
                int& width, int& height, int& channels);

    // Decode JPEG directly to pre-allocated buffer (zero-copy optimization)
    // buffer_size must be >= width * height * channels
    bool decode_to_buffer(const std::string& filename,
                         uint8_t* output_buffer,
                         size_t buffer_size,
                         int& width, int& height, int& channels);

    // Get image info without decoding
    bool get_image_info(const std::string& filename,
                       int& width, int& height, int& channels);

    // Decode with fast DCT algorithm (faster but slightly lower quality)
    bool decode_fast(const std::string& filename,
                     std::vector<uint8_t>& output,
                     int& width, int& height, int& channels);

    // Check if initialized
    bool isInitialized() const { return initialized_; }

private:
    void cleanup();

    tjhandle handle_;
    bool initialized_;
};

#endif // TURBOJPEG_DECODER_H
