/**
 * Parallel JPEG Encoder - Split large image into tiles and compress in parallel
 * This can achieve 8-10x speedup on multi-core CPUs
 */

#include <turbojpeg.h>
#include <cstring>
#include <vector>
#include <thread>
#include <algorithm>

// DLL export macro
#ifdef _WIN32
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT
#endif

extern "C" {

struct TileJPEG {
    unsigned char* data;
    unsigned long size;
    int tileX;
    int tileY;
};

/**
 * Encode a single tile in parallel
 */
void encodeTile(const int* rgbData, int imageWidth, int imageHeight,
                int tileX, int tileY, int tileWidth, int tileHeight,
                int quality, TileJPEG* output) {
    
    // Allocate BGR buffer for this tile
    int tilePixels = tileWidth * tileHeight;
    unsigned char* bgrTile = new unsigned char[tilePixels * 3];
    
    // Convert INT_RGB to BGR for this tile
    for (int ty = 0; ty < tileHeight; ty++) {
        int srcY = tileY + ty;
        if (srcY >= imageHeight) break;
        
        for (int tx = 0; tx < tileWidth; tx++) {
            int srcX = tileX + tx;
            if (srcX >= imageWidth) break;
            
            int srcIdx = srcY * imageWidth + srcX;
            int dstIdx = ty * tileWidth + tx;
            
            int argb = rgbData[srcIdx];
            bgrTile[dstIdx * 3 + 0] = (unsigned char)((argb) & 0xFF);        // B
            bgrTile[dstIdx * 3 + 1] = (unsigned char)((argb >> 8) & 0xFF);   // G
            bgrTile[dstIdx * 3 + 2] = (unsigned char)((argb >> 16) & 0xFF);  // R
        }
    }
    
    // Compress tile
    tjhandle tj = tjInitCompress();
    unsigned char* jpegBuf = nullptr;
    unsigned long jpegSize = 0;
    
    int ret = tjCompress2(
        tj,
        bgrTile,
        tileWidth,
        0,  // pitch
        tileHeight,
        TJPF_BGR,
        &jpegBuf,
        &jpegSize,
        TJSAMP_420,
        quality,
        TJFLAG_FASTDCT
    );
    
    delete[] bgrTile;
    
    if (ret == 0) {
        output->data = jpegBuf;
        output->size = jpegSize;
        output->tileX = tileX;
        output->tileY = tileY;
    } else {
        output->data = nullptr;
        output->size = 0;
        tjDestroy(tj);
    }
}

/**
 * Parallel tile encoding with automatic thread management
 * @param rgbData INT_RGB pixel data (4 bytes per pixel)
 * @param width Image width
 * @param height Image height
 * @param quality JPEG quality (1-100)
 * @param tileSize Tile size (e.g., 4096x4096)
 * @param numTiles Output: number of tiles generated
 * @return Array of TileJPEG (caller must free)
 */
DLL_EXPORT TileJPEG* EncodeParallelTiles(const int* rgbData, 
                                         int width, 
                                         int height,
                                         int quality,
                                         int tileSize,
                                         int* numTiles) {
    if (!rgbData || width <= 0 || height <= 0 || tileSize <= 0) {
        *numTiles = 0;
        return nullptr;
    }
    
    // Calculate tile grid
    int tilesX = (width + tileSize - 1) / tileSize;
    int tilesY = (height + tileSize - 1) / tileSize;
    int totalTiles = tilesX * tilesY;
    
    *numTiles = totalTiles;
    
    // Allocate output array
    TileJPEG* tiles = new TileJPEG[totalTiles];
    
    // Parallel encoding
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads <= 0) numThreads = 8;
    
    std::vector<std::thread> threads;
    std::atomic<int> tileIndex(0);
    
    auto workerFunc = [&]() {
        while (true) {
            int idx = tileIndex.fetch_add(1);
            if (idx >= totalTiles) break;
            
            int tx = (idx % tilesX) * tileSize;
            int ty = (idx / tilesX) * tileSize;
            int tw = std::min(tileSize, width - tx);
            int th = std::min(tileSize, height - ty);
            
            encodeTile(rgbData, width, height, tx, ty, tw, th, quality, &tiles[idx]);
        }
    };
    
    // Launch worker threads
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(workerFunc);
    }
    
    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }
    
    return tiles;
}

/**
 * Free tile array
 */
DLL_EXPORT void FreeTileArray(TileJPEG* tiles, int numTiles) {
    if (!tiles) return;
    
    for (int i = 0; i < numTiles; i++) {
        if (tiles[i].data) {
            tjFree(tiles[i].data);
        }
    }
    
    delete[] tiles;
}

} // extern "C"
