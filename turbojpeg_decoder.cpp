#include "turbojpeg_decoder.h"
#include <turbojpeg.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

TurboJpegDecoder::TurboJpegDecoder()
    : handle_(nullptr)
    , initialized_(false) {
}

TurboJpegDecoder::~TurboJpegDecoder() {
    cleanup();
}

bool TurboJpegDecoder::init() {
    if (initialized_) {
        return true;
    }

    // Initialize TurboJPEG decompressor
    handle_ = tjInitDecompress();
    if (!handle_) {
        std::cerr << "Failed to initialize TurboJPEG decompressor" << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

bool TurboJpegDecoder::decode(const std::string& filename,
                              std::vector<uint8_t>& output,
                              int& width, int& height, int& channels) {
    if (!initialized_) {
        std::cerr << "Decoder not initialized" << std::endl;
        return false;
    }

    // 1. Read JPEG file into memory
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> jpeg_buffer(size);
    if (!file.read(reinterpret_cast<char*>(jpeg_buffer.data()), size)) {
        std::cerr << "Failed to read file" << std::endl;
        return false;
    }
    file.close();

    // 2. Get JPEG image information
    int jpeg_subsocks = 0;
    int jpeg_width = 0;
    int jpeg_height = 0;

    int retval = tjDecompressHeader3(
        handle_,
        jpeg_buffer.data(),
        static_cast<unsigned long>(size),
        &jpeg_width,
        &jpeg_height,
        &jpeg_subsocks,
        &channels
    );

    if (retval < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Failed to read JPEG header: %s",
                tjGetErrorStr());
        std::cerr << error_msg << std::endl;
        return false;
    }

    width = jpeg_width;
    height = jpeg_height;

    // Determine channels based on chrominance subsampling
    // TJSAMP_GRAY = 4 means grayscale image
    // Other values (0, 1, 2, 3, 5) mean color images
    if (jpeg_subsocks == 4) {  // TJSAMP_GRAY
        channels = 1;
    } else {
        channels = 3;  // Color image
    }

    // 3. Allocate output buffer
    // TurboJPEG uses TJPF_BGR which is BGR, bytes per pixel = 3
    const int bytes_per_pixel = (channels == 1) ? 1 : 3;
    const int pitch = width * bytes_per_pixel;  // No padding between rows
    const size_t buffer_size = pitch * height;

    output.resize(buffer_size);

    // 4. Decode JPEG directly to BGR format
    // TJPF_BGR = 2 (BGR byte order)
    // TJFLAG_ACCURATEDCT = 16 (Use accurate DCT/IDCT algorithms)
    // TJFLAG_FASTDCT = 8 (Use fast DCT/IDCT algorithms)
    const int pixel_format = (channels == 1) ? TJPF_GRAY : TJPF_BGR;
    const int flags = TJFLAG_ACCURATEDCT;  // High quality

    retval = tjDecompress2(
        handle_,
        jpeg_buffer.data(),
        static_cast<unsigned long>(size),
        output.data(),
        width,
        pitch,
        height,
        pixel_format,
        flags
    );

    if (retval < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Failed to decompress JPEG: %s",
                tjGetErrorStr());
        std::cerr << error_msg << std::endl;
        return false;
    }

    return true;
}

bool TurboJpegDecoder::decode_to_buffer(const std::string& filename,
                                       uint8_t* output_buffer,
                                       size_t buffer_size,
                                       int& width, int& height, int& channels) {
    if (!initialized_) {
        std::cerr << "Decoder not initialized" << std::endl;
        return false;
    }

    if (!output_buffer) {
        std::cerr << "Output buffer is null" << std::endl;
        return false;
    }

    // 1. Read JPEG file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> jpeg_buffer(size);
    if (!file.read(reinterpret_cast<char*>(jpeg_buffer.data()), size)) {
        std::cerr << "Failed to read file" << std::endl;
        return false;
    }
    file.close();

    // 2. Get JPEG info
    int jpeg_subsocks = 0;
    int jpeg_width = 0;
    int jpeg_height = 0;

    int retval = tjDecompressHeader3(
        handle_,
        jpeg_buffer.data(),
        static_cast<unsigned long>(size),
        &jpeg_width,
        &jpeg_height,
        &jpeg_subsocks,
        &channels
    );

    if (retval < 0) {
        std::cerr << "Failed to read JPEG header" << std::endl;
        return false;
    }

    width = jpeg_width;
    height = jpeg_height;

    // Determine channels from subsampling
    if (jpeg_subsocks == 4) {  // TJSAMP_GRAY
        channels = 1;
    } else {
        channels = 3;
    }

    // 3. Calculate required buffer size
    const int bytes_per_pixel = (channels == 1) ? 1 : 3;
    const size_t required_size = width * height * bytes_per_pixel;

    if (buffer_size < required_size) {
        std::cerr << "Output buffer too small: need " << required_size
                  << ", got " << buffer_size << std::endl;
        return false;
    }

    // 4. Decode directly to output buffer (zero-copy from decoder perspective)
    const int pitch = width * bytes_per_pixel;
    const int pixel_format = (channels == 1) ? TJPF_GRAY : TJPF_BGR;
    const int flags = TJFLAG_ACCURATEDCT;

    retval = tjDecompress2(
        handle_,
        jpeg_buffer.data(),
        static_cast<unsigned long>(size),
        output_buffer,  // 直接解码到用户提供的 buffer
        width,
        pitch,
        height,
        pixel_format,
        flags
    );

    if (retval < 0) {
        std::cerr << "Failed to decompress JPEG" << std::endl;
        return false;
    }

    return true;
}

bool TurboJpegDecoder::get_image_info(const std::string& filename,
                                      int& width, int& height, int& channels) {
    if (!initialized_) {
        std::cerr << "Decoder not initialized" << std::endl;
        return false;
    }

    // Read JPEG file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> jpeg_buffer(size);
    if (!file.read(reinterpret_cast<char*>(jpeg_buffer.data()), size)) {
        std::cerr << "Failed to read file" << std::endl;
        return false;
    }
    file.close();

    // Get info
    int jpeg_subsocks = 0;
    int jpeg_width = 0;
    int jpeg_height = 0;

    int retval = tjDecompressHeader3(
        handle_,
        jpeg_buffer.data(),
        static_cast<unsigned long>(size),
        &jpeg_width,
        &jpeg_height,
        &jpeg_subsocks,
        &channels
    );

    if (retval < 0) {
        std::cerr << "Failed to read JPEG header" << std::endl;
        return false;
    }

    width = jpeg_width;
    height = jpeg_height;

    // Determine channels from subsampling
    if (jpeg_subsocks == 4) {  // TJSAMP_GRAY
        channels = 1;
    } else {
        channels = 3;
    }

    return true;
}

bool TurboJpegDecoder::decode_fast(const std::string& filename,
                                  std::vector<uint8_t>& output,
                                  int& width, int& height, int& channels) {
    if (!initialized_) {
        std::cerr << "Decoder not initialized" << std::endl;
        return false;
    }

    // 1. Read JPEG file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> jpeg_buffer(size);
    if (!file.read(reinterpret_cast<char*>(jpeg_buffer.data()), size)) {
        std::cerr << "Failed to read file" << std::endl;
        return false;
    }
    file.close();

    // 2. Get JPEG info
    int jpeg_subsocks = 0;
    int jpeg_width = 0;
    int jpeg_height = 0;

    int retval = tjDecompressHeader3(
        handle_,
        jpeg_buffer.data(),
        static_cast<unsigned long>(size),
        &jpeg_width,
        &jpeg_height,
        &jpeg_subsocks,
        &channels
    );

    if (retval < 0) {
        std::cerr << "Failed to read JPEG header" << std::endl;
        return false;
    }

    width = jpeg_width;
    height = jpeg_height;

    // Determine channels from subsampling
    if (jpeg_subsocks == 4) {  // TJSAMP_GRAY
        channels = 1;
    } else {
        channels = 3;
    }

    // 3. Allocate output buffer
    const int bytes_per_pixel = (channels == 1) ? 1 : 3;
    const int pitch = width * bytes_per_pixel;
    const size_t buffer_size = pitch * height;

    output.resize(buffer_size);

    // 4. Decode with FAST DCT algorithm (TJFLAG_FASTDCT)
    // Faster but slightly lower quality
    const int pixel_format = (channels == 1) ? TJPF_GRAY : TJPF_BGR;
    const int flags = TJFLAG_FASTDCT;  // Fast DCT algorithm

    retval = tjDecompress2(
        handle_,
        jpeg_buffer.data(),
        static_cast<unsigned long>(size),
        output.data(),
        width,
        pitch,
        height,
        pixel_format,
        flags
    );

    if (retval < 0) {
        std::cerr << "Failed to decompress JPEG (fast)" << std::endl;
        return false;
    }

    return true;
}

void TurboJpegDecoder::cleanup() {
    if (handle_) {
        tjDestroy(handle_);
        handle_ = nullptr;
    }

    initialized_ = false;
}
