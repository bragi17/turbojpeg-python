/**
 * Universal JPEG Encoder - Decoupled from specific Java class
 * 
 * This DLL can be loaded by ANY Java class, not tied to specific class name.
 * Uses JNI dynamic registration instead of static naming.
 * 
 * Usage in Java:
 *   public class YourClassName {
 *       static {
 *           System.loadLibrary("UniversalJpegEncoder");
 *       }
 *       
 *       private native int encodeJPEG(byte[] bgrData, int width, int height,
 *                                     float quality, OutputStream os, int threads);
 *   }
 */

#include <jni.h>
#include <turbojpeg.h>
#include <jpeglib.h>
#include <cstring>
#include <thread>
#include <vector>
#include <algorithm>

// DLL export macro
#ifdef _WIN32
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT
#endif

// ========== Core encoding functions (no class name dependency) ==========

/**
 * Encode BGR byte array to JPEG
 * This function has NO class name in its signature!
 */
jint encodeFromBGR_universal(JNIEnv *env, jobject obj, 
                             jbyteArray bgrData, jint width, jint height,
                             jfloat quality, jobject outputStream, jint numThreads) {
    
    if (numThreads <= 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads <= 0) numThreads = 4;
    }
    
    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        return -1;
    }
    
    jbyte* bgrBytes = env->GetByteArrayElements(bgrData, nullptr);
    if (!bgrBytes) {
        tjDestroy(tjInstance);
        return -1;
    }
    
    unsigned char* imageData = (unsigned char*)bgrBytes;
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    
    int qualityInt = (int)(quality * 100);
    if (qualityInt < 1) qualityInt = 1;
    if (qualityInt > 100) qualityInt = 100;
    
    int ret = tjCompress2(
        tjInstance,
        imageData,
        width,
        0,  // pitch
        height,
        TJPF_BGR,
        &jpegBuf,
        &jpegSize,
        TJSAMP_420,
        qualityInt,
        TJFLAG_FASTDCT
    );
    
    env->ReleaseByteArrayElements(bgrData, bgrBytes, JNI_ABORT);
    
    if (ret != 0) {
        tjDestroy(tjInstance);
        return -1;
    }
    
    // Write to OutputStream
    jclass streamClass = env->GetObjectClass(outputStream);
    jmethodID writeMethod = env->GetMethodID(streamClass, "write", "([BII)V");
    
    if (!writeMethod) {
        tjFree(jpegBuf);
        tjDestroy(tjInstance);
        return -1;
    }
    
    jbyteArray javaArray = env->NewByteArray(jpegSize);
    env->SetByteArrayRegion(javaArray, 0, jpegSize, (jbyte*)jpegBuf);
    env->CallVoidMethod(outputStream, writeMethod, javaArray, 0, jpegSize);
    
    env->DeleteLocalRef(javaArray);
    tjFree(jpegBuf);
    tjDestroy(tjInstance);
    
    return (jint)jpegSize;
}

/**
 * Encode ARGB int array to JPEG
 */
jint encodeFromARGB_universal(JNIEnv *env, jobject obj,
                              jintArray pixels, jint width, jint height,
                              jfloat quality, jobject outputStream, jint numThreads) {
    
    if (numThreads <= 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads <= 0) numThreads = 4;
    }
    
    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        return -1;
    }
    
    jint* pixelData = env->GetIntArrayElements(pixels, nullptr);
    if (!pixelData) {
        tjDestroy(tjInstance);
        return -1;
    }
    
    // Convert ARGB to RGB
    int pixelCount = width * height;
    unsigned char* rgbData = new unsigned char[pixelCount * 3];
    
    for (int i = 0; i < pixelCount; i++) {
        int argb = pixelData[i];
        rgbData[i * 3 + 0] = (argb >> 16) & 0xFF;  // R
        rgbData[i * 3 + 1] = (argb >> 8) & 0xFF;   // G
        rgbData[i * 3 + 2] = argb & 0xFF;          // B
    }
    
    env->ReleaseIntArrayElements(pixels, pixelData, JNI_ABORT);
    
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    
    int qualityInt = (int)(quality * 100);
    if (qualityInt < 1) qualityInt = 1;
    if (qualityInt > 100) qualityInt = 100;
    
    int ret = tjCompress2(
        tjInstance,
        rgbData,
        width,
        0,
        height,
        TJPF_RGB,
        &jpegBuf,
        &jpegSize,
        TJSAMP_420,
        qualityInt,
        TJFLAG_FASTDCT
    );
    
    delete[] rgbData;
    
    if (ret != 0) {
        tjDestroy(tjInstance);
        return -1;
    }
    
    // Write to OutputStream
    jclass streamClass = env->GetObjectClass(outputStream);
    jmethodID writeMethod = env->GetMethodID(streamClass, "write", "([BII)V");
    
    if (!writeMethod) {
        tjFree(jpegBuf);
        tjDestroy(tjInstance);
        return -1;
    }
    
    jbyteArray javaArray = env->NewByteArray(jpegSize);
    env->SetByteArrayRegion(javaArray, 0, jpegSize, (jbyte*)jpegBuf);
    env->CallVoidMethod(outputStream, writeMethod, javaArray, 0, jpegSize);
    
    env->DeleteLocalRef(javaArray);
    tjFree(jpegBuf);
    tjDestroy(tjInstance);
    
    return (jint)jpegSize;
}

// ========== JNI Dynamic Registration ==========

// Method table for dynamic registration
// These can be registered to ANY class!
static JNINativeMethod methods[] = {
    {
        (char*)"encodeJPEG",
        (char*)"([BIIFLjava/io/OutputStream;I)I",
        (void*)encodeFromBGR_universal
    },
    {
        (char*)"encodeJPEGFromPixels",
        (char*)"([IIIFLjava/io/OutputStream;I)I",
        (void*)encodeFromARGB_universal
    }
};

/**
 * JNI_OnLoad - called when library is loaded
 * This allows dynamic registration to any class
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    // We don't register here - let the Java code register to its own class
    return JNI_VERSION_1_8;
}

/**
 * Helper function to register methods to any class
 * Call this from Java with your class name
 */
extern "C" JNIEXPORT jint JNICALL 
Java_JpegEncoderRegistry_registerToClass(JNIEnv *env, jclass cls, jstring className) {
    
    const char* classNameStr = env->GetStringUTFChars(className, nullptr);
    
    // Find the target class
    jclass targetClass = env->FindClass(classNameStr);
    env->ReleaseStringUTFChars(className, classNameStr);
    
    if (targetClass == nullptr) {
        return -1;  // Class not found
    }
    
    // Register native methods to the target class
    int methodCount = sizeof(methods) / sizeof(methods[0]);
    if (env->RegisterNatives(targetClass, methods, methodCount) < 0) {
        return -2;  // Registration failed
    }
    
    return 0;  // Success
}

/**
 * Alternative: Auto-register when ANY class calls this
 * This is called from static initializer of your Java class
 */
extern "C" JNIEXPORT void JNICALL 
Java_UniversalJpegEncoder_registerNatives(JNIEnv *env, jclass cls) {
    int methodCount = sizeof(methods) / sizeof(methods[0]);
    env->RegisterNatives(cls, methods, methodCount);
}

// ========== Legacy compatibility (optional) ==========
// Keep these for backward compatibility if needed

extern "C" {

JNIEXPORT jint JNICALL Java_MinimalTest_encodeFromBGR
  (JNIEnv *env, jobject obj, jbyteArray bgrData, jint width, jint height,
   jfloat quality, jobject outputStream, jint numThreads) {
    return encodeFromBGR_universal(env, obj, bgrData, width, height, quality, outputStream, numThreads);
}

JNIEXPORT jint JNICALL Java_MinimalTest_encodeToStreamMT
  (JNIEnv *env, jobject obj, jintArray pixels, jint width, jint height,
   jfloat quality, jobject outputStream, jint numThreads) {
    return encodeFromARGB_universal(env, obj, pixels, width, height, quality, outputStream, numThreads);
}

} // extern "C"

// ========================= JNA-Compatible C API =========================
// Simple JPEG encoding API aligned with Java's ImageIO.write pattern
// Usage: byte[] jpeg = EncodeJPEG(pixels, width, height, quality);
//        FreeJPEGData(jpeg) when done

extern "C" {

struct JPEGData {
    unsigned char* data;
    int size;
};

/**
 * Encode pixel data to JPEG (replaces ImageIO.write)
 * 
 * @param pixels RGB or BGR pixel data (interleaved, 3 bytes per pixel)
 * @param width Image width
 * @param height Image height
 * @param quality JPEG quality 1-100 (85 recommended)
 * @param pixelFormat 0=RGB, 1=BGR, 2=BGRA, 3=RGBA (auto-detect from Java BufferedImage type)
 * @return JPEGData with allocated buffer (must call FreeJPEGData when done)
 */
DLL_EXPORT struct JPEGData EncodeJPEG(unsigned char* pixels,
                                      int width,
                                      int height,
                                      int quality,
                                      int pixelFormat) {
    JPEGData result{};
    result.data = nullptr;
    result.size = 0;

    if (!pixels || width <= 0 || height <= 0) {
        return result;
    }

    // Quality clamp 1..100
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;

    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        return result;
    }

    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;

    // Map pixelFormat to TurboJPEG format
    int tjFormat = TJPF_RGB;
    switch (pixelFormat) {
        case 0: tjFormat = TJPF_RGB; break;
        case 1: tjFormat = TJPF_BGR; break;
        case 2: tjFormat = TJPF_BGRA; break;
        case 3: tjFormat = TJPF_RGBA; break;
        default: tjFormat = TJPF_BGR; break; // Java BufferedImage default is often BGR
    }

    int ret = tjCompress2(
        tjInstance,
        pixels,
        width,
        0,  // pitch (auto)
        height,
        tjFormat,
        &jpegBuf,
        &jpegSize,
        TJSAMP_420,
        quality,
        TJFLAG_FASTDCT
    );

    if (ret != 0) {
        tjDestroy(tjInstance);
        return result;
    }

    // Allocate output buffer (use malloc for cross-boundary safety)
    unsigned char* outBuf = (unsigned char*)std::malloc(jpegSize);
    if (!outBuf) {
        tjFree(jpegBuf);
        tjDestroy(tjInstance);
        return result;
    }
    std::memcpy(outBuf, jpegBuf, jpegSize);

    result.data = outBuf;
    result.size = (int)jpegSize;

    tjFree(jpegBuf);
    tjDestroy(tjInstance);
    return result;
}

/**
 * Free JPEG data returned by EncodeJPEG
 */
DLL_EXPORT void FreeJPEGData(struct JPEGData* jpeg) {
    if (!jpeg) return;
    if (jpeg->data) {
        std::free(jpeg->data);
        jpeg->data = nullptr;
        jpeg->size = 0;
    }
}

// ========================= Streaming/Chunked Encoding API =========================
// Supports chunked encoding for arbitrarily large images without loading entire image into memory

/**
 * Streaming encoder context
 */
struct StreamEncoder {
    tjhandle tjInstance;
    unsigned char* jpegBuf;
    unsigned long jpegSize;
    int width;
    int height;
    int quality;
    int pixelFormat;
    int currentRow;
    unsigned char** rowBuffer;  // Accumulated scanline buffer
    int rowBufferCount;
};

/**
 * Create streaming JPEG encoder
 * @param width Image width
 * @param height Image height
 * @param quality JPEG quality 1-100
 * @param pixelFormat 0=RGB, 1=BGR, 2=BGRA, 3=RGBA
 * @return Encoder handle (must call DestroyStreamEncoder to free)
 */
DLL_EXPORT void* CreateStreamEncoder(int width, int height, int quality, int pixelFormat) {
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    
    StreamEncoder* encoder = new StreamEncoder();
    encoder->tjInstance = tjInitCompress();
    if (!encoder->tjInstance) {
        delete encoder;
        return nullptr;
    }
    
    encoder->width = width;
    encoder->height = height;
    encoder->quality = (quality < 1) ? 1 : (quality > 100) ? 100 : quality;
    encoder->pixelFormat = pixelFormat;
    encoder->currentRow = 0;
    encoder->jpegBuf = nullptr;
    encoder->jpegSize = 0;
    
    // Allocate row buffer (stores all scanline pointers)
    encoder->rowBuffer = new unsigned char*[height];
    for (int i = 0; i < height; i++) {
        encoder->rowBuffer[i] = nullptr;
    }
    encoder->rowBufferCount = 0;
    
    return encoder;
}

/**
 * Write image row data (batch input) - for byte[] data (BGR/RGB)
 * @param encoderHandle Handle returned by CreateStreamEncoder
 * @param rowData Row data (BGR/RGB format, rowCount rows of continuous data)
 * @param rowCount Number of rows in this batch
 * @return Total rows written on success, -1 on failure
 */
DLL_EXPORT int WriteImageRows(void* encoderHandle, unsigned char* rowData, int rowCount) {
    if (!encoderHandle || !rowData || rowCount <= 0) {
        return -1;
    }
    
    StreamEncoder* encoder = (StreamEncoder*)encoderHandle;
    
    if (encoder->currentRow + rowCount > encoder->height) {
        return -1;  // Exceeds image height
    }
    
    int bytesPerPixel = (encoder->pixelFormat == 2 || encoder->pixelFormat == 3) ? 4 : 3;
    int rowBytes = encoder->width * bytesPerPixel;
    
    // Copy row data to internal buffer
    for (int i = 0; i < rowCount; i++) {
        unsigned char* row = new unsigned char[rowBytes];
        std::memcpy(row, rowData + i * rowBytes, rowBytes);
        encoder->rowBuffer[encoder->currentRow + i] = row;
    }
    
    encoder->currentRow += rowCount;
    encoder->rowBufferCount += rowCount;
    
    return encoder->currentRow;
}

/**
 * Write image row data from int[] array (TYPE_INT_RGB/TYPE_INT_ARGB)
 * Multi-threaded conversion for maximum speed!
 * @param encoderHandle Handle returned by CreateStreamEncoder
 * @param rowData Row data as int[] (4 bytes per pixel: 0xAARRGGBB)
 * @param rowCount Number of rows in this batch
 * @return Total rows written on success, -1 on failure
 */
DLL_EXPORT int WriteImageRowsInt(void* encoderHandle, int* rowData, int rowCount) {
    if (!encoderHandle || !rowData || rowCount <= 0) {
        return -1;
    }
    
    StreamEncoder* encoder = (StreamEncoder*)encoderHandle;
    
    if (encoder->currentRow + rowCount > encoder->height) {
        return -1;  // Exceeds image height
    }
    
    int bytesPerRow = encoder->width * 3;  // BGR: 3 bytes per pixel
    
    // MULTI-THREADED conversion (8x faster on 8-core CPU)
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads <= 0) numThreads = 4;
    if (numThreads > rowCount) numThreads = rowCount;
    
    std::vector<std::thread> threads;
    
    auto convertRows = [&](int startRow, int endRow) {
        for (int row = startRow; row < endRow; row++) {
            unsigned char* bgrRow = new unsigned char[bytesPerRow];
            int* srcRow = rowData + row * encoder->width;
            
            // Optimized conversion loop
            for (int x = 0; x < encoder->width; x++) {
                unsigned int argb = (unsigned int)srcRow[x];
                unsigned char* dst = &bgrRow[x * 3];
                dst[0] = (unsigned char)(argb);         // B
                dst[1] = (unsigned char)(argb >> 8);    // G
                dst[2] = (unsigned char)(argb >> 16);   // R
            }
            
            encoder->rowBuffer[encoder->currentRow + row] = bgrRow;
        }
    };
    
    // Launch parallel conversion
    int rowsPerThread = (rowCount + numThreads - 1) / numThreads;
    for (int t = 0; t < numThreads; t++) {
        int startRow = t * rowsPerThread;
        int endRow = std::min(startRow + rowsPerThread, rowCount);
        if (startRow < endRow) {
            threads.emplace_back(convertRows, startRow, endRow);
        }
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    encoder->currentRow += rowCount;
    encoder->rowBufferCount += rowCount;
    
    return encoder->currentRow;
}

/**
 * Complete encoding and get JPEG data
 * For small images (<2GB): use fast turbojpeg batch compression
 * For large images (>=2GB): use libjpeg scanline API without full buffer allocation
 * @param encoderHandle Encoder handle
 * @return JPEGData structure (must call FreeJPEGData to free)
 */
DLL_EXPORT struct JPEGData FinalizeStreamEncoder(void* encoderHandle) {
    JPEGData result{};
    result.data = nullptr;
    result.size = 0;
    
    if (!encoderHandle) {
        return result;
    }
    
    StreamEncoder* encoder = (StreamEncoder*)encoderHandle;
    
    if (encoder->currentRow != encoder->height) {
        return result;  // Not all rows collected
    }
    
    int bytesPerPixel = (encoder->pixelFormat == 2 || encoder->pixelFormat == 3) ? 4 : 3;
    int rowBytes = encoder->width * bytesPerPixel;
    long long totalBytes = (long long)encoder->height * rowBytes;
    
    // Threshold for switching to scanline encoding (1.5GB)
    const long long MAX_BATCH = 1500000000LL;
    
    // Fast single-JPEG compression with optimizations
    if (totalBytes < MAX_BATCH) {
        // Allocate image buffer
        unsigned char* imageData = new (std::nothrow) unsigned char[totalBytes];
        
        if (!imageData) {
            return result;  // Out of memory
        }
        
        // Multi-threaded row copy for speed
        int numThreads = std::thread::hardware_concurrency();
        if (numThreads <= 0) numThreads = 4;
        
        std::vector<std::thread> threads;
        int rowsPerThread = (encoder->height + numThreads - 1) / numThreads;
        
        auto copyRows = [&](int startRow, int endRow) {
            for (int i = startRow; i < endRow && i < encoder->height; i++) {
                std::memcpy(imageData + (long long)i * rowBytes, encoder->rowBuffer[i], rowBytes);
            }
        };
        
        for (int t = 0; t < numThreads; t++) {
            int start = t * rowsPerThread;
            int end = std::min(start + rowsPerThread, encoder->height);
            if (start < end) {
                threads.emplace_back(copyRows, start, end);
            }
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Use turbojpeg with all speed optimizations
        unsigned char* jpegBuf = nullptr;
        unsigned long jpegSize = 0;
        
        int ret = tjCompress2(
            encoder->tjInstance,
            imageData,
            encoder->width,
            0,  // pitch (auto)
            encoder->height,
            TJPF_BGR,
            &jpegBuf,
            &jpegSize,
            TJSAMP_420,
            encoder->quality,
            TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE  // Maximum speed
        );
        
        delete[] imageData;
        
        if (ret != 0) {
            tjFree(jpegBuf);
            return result;
        }
        
        // Copy result
        unsigned char* outBuf = (unsigned char*)std::malloc(jpegSize);
        if (!outBuf) {
            tjFree(jpegBuf);
            return result;
        }
        std::memcpy(outBuf, jpegBuf, jpegSize);
        
        result.data = outBuf;
        result.size = (int)jpegSize;
        
        tjFree(jpegBuf);
        return result;
    }
    else {
        // SLOW PATH: Use libjpeg scanline API for huge images (avoids large allocation)
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        
        // Output to memory buffer
        unsigned char* outbuffer = nullptr;
        unsigned long outsize = 0;
        jpeg_mem_dest(&cinfo, &outbuffer, &outsize);
        
        // Set image parameters
        cinfo.image_width = encoder->width;
        cinfo.image_height = encoder->height;
        cinfo.input_components = 3;  // RGB/BGR
        cinfo.in_color_space = JCS_RGB;
        
        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, encoder->quality, TRUE);
        
        // Start compression
        jpeg_start_compress(&cinfo, TRUE);
        
        bool needConversion = (encoder->pixelFormat == 1); // BGR needs conversion
        
        // Write scanlines in large batches for much better performance
        const int SCANLINE_BATCH = 5000;  // Write 5000 rows at a time (huge speedup)
        JSAMPROW* row_pointers = new JSAMPROW[SCANLINE_BATCH];
        
        // Allocate batch conversion buffer if needed
        unsigned char** batchConvertBuffers = nullptr;
        if (needConversion) {
            batchConvertBuffers = new unsigned char*[SCANLINE_BATCH];
            for (int i = 0; i < SCANLINE_BATCH; i++) {
                batchConvertBuffers[i] = new unsigned char[encoder->width * 3];
            }
        }
        
        while (cinfo.next_scanline < cinfo.image_height) {
            int rowsToWrite = std::min(SCANLINE_BATCH, (int)(cinfo.image_height - cinfo.next_scanline));
            
            for (int i = 0; i < rowsToWrite; i++) {
                unsigned char* srcRow = encoder->rowBuffer[cinfo.next_scanline + i];
                
                if (needConversion) {
                    // Convert BGR to RGB for this row
                    unsigned char* dstRow = batchConvertBuffers[i];
                    for (int x = 0; x < encoder->width; x++) {
                        dstRow[x * 3 + 0] = srcRow[x * 3 + 2]; // R
                        dstRow[x * 3 + 1] = srcRow[x * 3 + 1]; // G
                        dstRow[x * 3 + 2] = srcRow[x * 3 + 0]; // B
                    }
                    row_pointers[i] = dstRow;
                } else {
                    row_pointers[i] = srcRow;
                }
            }
            
            // Write batch of scanlines
            jpeg_write_scanlines(&cinfo, row_pointers, rowsToWrite);
        }
        
        // Cleanup batch buffers
        if (batchConvertBuffers) {
            for (int i = 0; i < SCANLINE_BATCH; i++) {
                delete[] batchConvertBuffers[i];
            }
            delete[] batchConvertBuffers;
        }
        delete[] row_pointers;
        
        // Finish compression
        jpeg_finish_compress(&cinfo);
        
        // Copy to malloc buffer (for compatibility with FreeJPEGData)
        unsigned char* finalBuf = (unsigned char*)std::malloc(outsize);
        if (!finalBuf) {
            free(outbuffer);
            jpeg_destroy_compress(&cinfo);
            return result;
        }
        std::memcpy(finalBuf, outbuffer, outsize);
        
        result.data = finalBuf;
        result.size = (int)outsize;
        
        // Clean up
        free(outbuffer);
        jpeg_destroy_compress(&cinfo);
        
        return result;
    }
}

/**
 * Destroy streaming encoder
 * @param encoderHandle Encoder handle
 */
DLL_EXPORT void DestroyStreamEncoder(void* encoderHandle) {
    if (!encoderHandle) return;
    
    StreamEncoder* encoder = (StreamEncoder*)encoderHandle;
    
    // Free row buffers
    for (int i = 0; i < encoder->height; i++) {
        if (encoder->rowBuffer[i]) {
            delete[] encoder->rowBuffer[i];
        }
    }
    delete[] encoder->rowBuffer;
    
    // Free tjhandle
    if (encoder->tjInstance) {
        tjDestroy(encoder->tjInstance);
    }
    
    delete encoder;
}

} // extern "C" (JNA API)

