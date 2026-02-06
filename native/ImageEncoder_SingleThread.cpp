#include <jni.h>
#include "include/turbojpeg.h"
#include <cstring>

extern "C" {

/**
 * JNI方法：将BufferedImage像素数据编码为JPG并写入OutputStream
 * 
 * @param pixels ARGB格式的像素数组
 * @param width 图像宽度
 * @param height 图像高度
 * @param quality 质量 (0.0-1.0)
 * @param outputStream Java OutputStream对象
 * @return 编码后的字节数，失败返回-1
 */
JNIEXPORT jint JNICALL Java_com_yourpackage_TurboJpegEncoder_encodeToStream
  (JNIEnv *env, jobject obj, jintArray pixels, jint width, jint height, 
   jfloat quality, jobject outputStream) {
    
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
    
    // 转换ARGB到RGB (去掉Alpha通道)
    long long pixelCount = (long long)width * height;
    unsigned char* rgbBuffer = new unsigned char[pixelCount * 3];
    
    // 优化：直接转换，避免额外拷贝
    for (long long i = 0; i < pixelCount; i++) {
        int argb = pixelData[i];
        rgbBuffer[i * 3 + 0] = (argb >> 16) & 0xFF; // R
        rgbBuffer[i * 3 + 1] = (argb >> 8) & 0xFF;  // G
        rgbBuffer[i * 3 + 2] = argb & 0xFF;         // B
    }
    
    env->ReleaseIntArrayElements(pixels, pixelData, JNI_ABORT);
    
    // TurboJPEG编码
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    int qualityInt = (int)(quality * 100);
    if (qualityInt < 1) qualityInt = 1;
    if (qualityInt > 100) qualityInt = 100;
    
    // 使用4:2:0采样和快速DCT以获得最佳性能
    int ret = tjCompress2(
        tjInstance, 
        rgbBuffer,          // 源图像数据
        width,              // 宽度
        0,                  // pitch (0=自动)
        height,             // 高度
        TJPF_RGB,           // 像素格式
        &jpegBuf,           // 输出缓冲区（自动分配）
        &jpegSize,          // 输出大小
        TJSAMP_420,         // 色度采样 (4:2:0最快)
        qualityInt,         // 质量
        TJFLAG_FASTDCT      // 使用快速DCT
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
    
    // 创建Java字节数组并传输数据
    jbyteArray javaArray = env->NewByteArray(jpegSize);
    env->SetByteArrayRegion(javaArray, 0, jpegSize, (jbyte*)jpegBuf);
    env->CallVoidMethod(outputStream, writeMethod, javaArray, 0, jpegSize);
    
    // 清理资源
    env->DeleteLocalRef(javaArray);
    tjFree(jpegBuf);
    tjDestroy(tjInstance);
    
    return (jint)jpegSize;
}

/**
 * 直接返回编码后的字节数组（适用于内存处理）
 */
JNIEXPORT jbyteArray JNICALL Java_com_yourpackage_TurboJpegEncoder_encodeToBytes
  (JNIEnv *env, jobject obj, jintArray pixels, jint width, jint height, jfloat quality) {
    
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
        return nullptr;
    }
    
    jbyteArray result = env->NewByteArray(jpegSize);
    env->SetByteArrayRegion(result, 0, jpegSize, (jbyte*)jpegBuf);
    
    tjFree(jpegBuf);
    tjDestroy(tjInstance);
    
    return result;
}

} // extern "C"

