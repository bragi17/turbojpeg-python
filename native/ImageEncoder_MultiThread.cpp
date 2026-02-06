#include <jni.h>
#include "include/turbojpeg.h"
#include <cstring>
#include <thread>
#include <vector>
#include <algorithm>

extern "C" {

/**
 * 多线程版本：并行转换ARGB到RGB
 * 
 * @param numThreads 线程数（1=单线程，0=自动检测CPU核心数）
 */
JNIEXPORT jint JNICALL Java_com_yourpackage_TurboJpegEncoder_encodeToStreamMT
  (JNIEnv *env, jobject obj, jintArray pixels, jint width, jint height, 
   jfloat quality, jobject outputStream, jint numThreads) {
    
    // 自动检测CPU核心数
    if (numThreads <= 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads <= 0) numThreads = 4; // 默认4线程
    }
    
    // 初始化TurboJPEG压缩器
    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        return -1;
    }
    
    // 获取Java像素数组
    jint* pixelData = env->GetIntArrayElements(pixels, nullptr);
    if (!pixelData) {
        tjDestroy(tjInstance);
        return -1;
    }
    
    long long pixelCount = (long long)width * height;
    unsigned char* rgbBuffer = new unsigned char[pixelCount * 3];
    
    // ========== 多线程转换ARGB到RGB ==========
    auto convertChunk = [pixelData, rgbBuffer](long long start, long long end) {
        for (long long i = start; i < end; i++) {
            int argb = pixelData[i];
            rgbBuffer[i * 3 + 0] = (argb >> 16) & 0xFF; // R
            rgbBuffer[i * 3 + 1] = (argb >> 8) & 0xFF;  // G
            rgbBuffer[i * 3 + 2] = argb & 0xFF;         // B
        }
    };
    
    if (numThreads == 1) {
        // 单线程模式
        convertChunk(0, pixelCount);
    } else {
        // 多线程模式
        std::vector<std::thread> threads;
        long long chunkSize = pixelCount / numThreads;
        
        for (int t = 0; t < numThreads; t++) {
            long long start = t * chunkSize;
            long long end = (t == numThreads - 1) ? pixelCount : (t + 1) * chunkSize;
            threads.emplace_back(convertChunk, start, end);
        }
        
        // 等待所有线程完成
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    env->ReleaseIntArrayElements(pixels, pixelData, JNI_ABORT);
    
    // ========== TurboJPEG编码（单线程，已通过SIMD优化） ==========
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    int qualityInt = (int)(quality * 100);
    if (qualityInt < 1) qualityInt = 1;
    if (qualityInt > 100) qualityInt = 100;
    
    int ret = tjCompress2(
        tjInstance, 
        rgbBuffer,
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
    
    delete[] rgbBuffer;
    
    if (ret != 0) {
        tjDestroy(tjInstance);
        return -1;
    }
    
    // 写入Java OutputStream
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
 * 单线程版本（兼容保留）
 */
JNIEXPORT jint JNICALL Java_com_yourpackage_TurboJpegEncoder_encodeToStream
  (JNIEnv *env, jobject obj, jintArray pixels, jint width, jint height, 
   jfloat quality, jobject outputStream) {
    
    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        return -1;
    }
    
    jint* pixelData = env->GetIntArrayElements(pixels, nullptr);
    if (!pixelData) {
        tjDestroy(tjInstance);
        return -1;
    }
    
    long long pixelCount = (long long)width * height;
    unsigned char* rgbBuffer = new unsigned char[pixelCount * 3];
    
    // 单线程转换
    for (long long i = 0; i < pixelCount; i++) {
        int argb = pixelData[i];
        rgbBuffer[i * 3 + 0] = (argb >> 16) & 0xFF;
        rgbBuffer[i * 3 + 1] = (argb >> 8) & 0xFF;
        rgbBuffer[i * 3 + 2] = argb & 0xFF;
    }
    
    env->ReleaseIntArrayElements(pixels, pixelData, JNI_ABORT);
    
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    int qualityInt = (int)(quality * 100);
    if (qualityInt < 1) qualityInt = 1;
    if (qualityInt > 100) qualityInt = 100;
    
    int ret = tjCompress2(tjInstance, rgbBuffer, width, 0, height, 
                          TJPF_RGB, &jpegBuf, &jpegSize, 
                          TJSAMP_420, qualityInt, TJFLAG_FASTDCT);
    
    delete[] rgbBuffer;
    
    if (ret != 0) {
        tjDestroy(tjInstance);
        return -1;
    }
    
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
 * 直接返回字节数组（多线程版本）
 */
JNIEXPORT jbyteArray JNICALL Java_com_yourpackage_TurboJpegEncoder_encodeToBytesMT
  (JNIEnv *env, jobject obj, jintArray pixels, jint width, jint height, 
   jfloat quality, jint numThreads) {
    
    if (numThreads <= 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads <= 0) numThreads = 4;
    }
    
    tjhandle tjInstance = tjInitCompress();
    if (!tjInstance) {
        return nullptr;
    }
    
    jint* pixelData = env->GetIntArrayElements(pixels, nullptr);
    if (!pixelData) {
        tjDestroy(tjInstance);
        return nullptr;
    }
    
    long long pixelCount = (long long)width * height;
    unsigned char* rgbBuffer = new unsigned char[pixelCount * 3];
    
    // 多线程转换
    auto convertChunk = [pixelData, rgbBuffer](long long start, long long end) {
        for (long long i = start; i < end; i++) {
            int argb = pixelData[i];
            rgbBuffer[i * 3 + 0] = (argb >> 16) & 0xFF;
            rgbBuffer[i * 3 + 1] = (argb >> 8) & 0xFF;
            rgbBuffer[i * 3 + 2] = argb & 0xFF;
        }
    };
    
    if (numThreads == 1) {
        convertChunk(0, pixelCount);
    } else {
        std::vector<std::thread> threads;
        long long chunkSize = pixelCount / numThreads;
        
        for (int t = 0; t < numThreads; t++) {
            long long start = t * chunkSize;
            long long end = (t == numThreads - 1) ? pixelCount : (t + 1) * chunkSize;
            threads.emplace_back(convertChunk, start, end);
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    env->ReleaseIntArrayElements(pixels, pixelData, JNI_ABORT);
    
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    int qualityInt = (int)(quality * 100);
    if (qualityInt < 1) qualityInt = 1;
    if (qualityInt > 100) qualityInt = 100;
    
    int ret = tjCompress2(tjInstance, rgbBuffer, width, 0, height, 
                          TJPF_RGB, &jpegBuf, &jpegSize, 
                          TJSAMP_420, qualityInt, TJFLAG_FASTDCT);
    
    delete[] rgbBuffer;
    
    if (ret != 0) {
        tjDestroy(tjInstance);
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(jpegSize);
    env->SetByteArrayRegion(result, 0, jpegSize, (jbyte*)jpegBuf);
    
    tjFree(jpegBuf);
    tjDestroy(tjInstance);
    
    return result;
}

} // extern "C"

