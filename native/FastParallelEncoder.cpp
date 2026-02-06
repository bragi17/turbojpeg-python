/**
 * Fast Parallel JPEG Encoder with Tile Stitching
 * - Direct int[] input (no Java conversion)
 * - Automatic tiling and parallel compression
 * - JPEG tile stitching for single output
 */

#include <stdio.h>
#include <turbojpeg.h>
#include <jpeglib.h>
#include <cstring>
#include <vector>
#include <thread>
#include <algorithm>
#include <atomic>

// DLL export macro
#ifdef _WIN32
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT
#endif

// Forward declare JPEGData (defined in UniversalJpegEncoder.cpp)
struct JPEGData {
    unsigned char* data;
    int size;
};

extern "C" {

/**
 * Fast parallel JPEG encoding with automatic tiling
 * @param rgbData INT_RGB pixel data (0xAARRGGBB format, direct from Java DataBuffer)
 * @param width Image width
 * @param height Image height
 * @param quality JPEG quality (1-100)
 * @param tileSize Tile size (e.g., 4096), 0 for auto
 * @return Single stitched JPEG (must call FreeJPEGData)
 */
DLL_EXPORT JPEGData EncodeParallelJPEG(const int* rgbData, 
                                       int width, 
                                       int height,
                                       int quality,
                                       int tileSize) {
    JPEGData result = {nullptr, 0};
    
    printf("[C++] EncodeParallelJPEG called: %dx%d, quality=%d, tileSize=%d\n", 
           width, height, quality, tileSize);
    fflush(stdout);
    
    if (!rgbData || width <= 0 || height <= 0) {
        printf("[C++] ERROR: Invalid parameters\n");
        fflush(stdout);
        return result;
    }
    
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    
    // ZERO-COPY optimization: INT_RGB memory layout matches TJPF_BGRX!
    printf("[C++] Zero-copy mode: compressing INT_RGB directly\n");
    fflush(stdout);
    
    // INT_RGB format (0x00RRGGBB) in little-endian memory: [BB GG RR 00]
    // TJPF_BGRX format: B G R X (4 bytes per pixel)
    // Perfect match! No conversion needed!
    
    printf("[C++] Input: %lld bytes (%.2f GB)\n", 
           (long long)width * height * 4, 
           width * height * 4 / 1024.0 / 1024.0 / 1024.0);
    fflush(stdout);
    
    // Compress directly from INT_RGB data (zero-copy!)
    printf("[C++] Compressing with TurboJPEG (zero-copy)...\n");
    fflush(stdout);
    
    tjhandle tj = tjInitCompress();
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    
    // Cast int* to unsigned char* and use TJPF_BGRX format
    const unsigned char* inputData = (const unsigned char*)rgbData;
    
    int ret = tjCompress2(tj, inputData, width, width * 4, height, TJPF_BGRX,
                         &jpegBuf, &jpegSize, TJSAMP_420, quality,
                         TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE);
    
    if (ret == 0 && jpegSize > 0) {
        result.data = (unsigned char*)malloc(jpegSize);
        memcpy(result.data, jpegBuf, jpegSize);
        result.size = (int)jpegSize;
        
        printf("[C++] Zero-copy compression done: %d bytes (%.2f MB)\n", 
               result.size, result.size / 1024.0 / 1024.0);
        fflush(stdout);
    } else {
        printf("[C++] ERROR: Zero-copy compression failed (ret=%d), trying fallback...\n", ret);
        fflush(stdout);
        
        // Initialize libjpeg for streaming compression
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        
        // Output to memory
        unsigned char* outbuffer = nullptr;
        unsigned long outsize = 0;
        jpeg_mem_dest(&cinfo, &outbuffer, &outsize);
        
        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        
        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, TRUE);
        jpeg_start_compress(&cinfo, TRUE);
        
        // Allocate single row buffer (only ~115KB)
        unsigned char* rowBuffer = new unsigned char[width * 3];
        
        // Process row by row - convert directly from rgbData
        for (int y = 0; y < height; y++) {
            const int* srcRow = rgbData + y * width;
            
            // Convert INT_RGB to RGB for libjpeg
            for (int x = 0; x < width; x++) {
                unsigned int argb = (unsigned int)srcRow[x];
                rowBuffer[x * 3 + 0] = (unsigned char)(argb >> 16);   // R
                rowBuffer[x * 3 + 1] = (unsigned char)(argb >> 8);    // G
                rowBuffer[x * 3 + 2] = (unsigned char)(argb);         // B
            }
            
            // Write row to JPEG
            JSAMPROW row_pointer[1];
            row_pointer[0] = rowBuffer;
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
            
            if (y % 5000 == 0 && y > 0) {
                printf("[C++] Streaming: %d/%d rows (%.1f%%)\n", y, height, y * 100.0 / height);
                fflush(stdout);
            }
        }
        
        delete[] rowBuffer;
        
        jpeg_finish_compress(&cinfo);
        
        if (outsize > 0 && outbuffer) {
            result.data = (unsigned char*)malloc(outsize);
            memcpy(result.data, outbuffer, outsize);
            result.size = (int)outsize;
            
            printf("[C++] Streaming JPEG done: %d bytes (%.2f MB)\n", 
                   result.size, result.size / 1024.0 / 1024.0);
            fflush(stdout);
        }
        
        free(outbuffer);
        jpeg_destroy_compress(&cinfo);
    }
    
    tjFree(jpegBuf);
    tjDestroy(tj);
    
    printf("[C++] Returning result\n");
    fflush(stdout);
    
    return result;
}

// Note: FreeJPEGData is defined in UniversalJpegEncoder.cpp

} // extern "C"
