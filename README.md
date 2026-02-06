# TurboJPEG Decoder

高性能 JPEG 解码器 - 比 OpenCV 快 1.58-1.99x

## 特性

- ⚡ **零拷贝解码** - 比OpenCV快1.58-1.99x
- ✅ **完全兼容OpenCV** - 相同的BGR格式（HWC, uint8）
- 📦 **静态链接** - 无需额外的DLL文件
- 🖼️ **支持大图** - 测试26416×14852像素图像
- 🎬 **视频流优化** - 1.77张/秒吞吐量

## 性能

测试图像: test.jpg (26416×14852像素, 17.75MB JPEG, 1122MB解码后)

| 方法 | 耗时 | 相对OpenCV |
|------|------|------------|
| **零拷贝 + 复用** | **564 ms** | **1.77x 快** |
| **零拷贝（单次）** | **578 ms** | **1.73x 快** |
| Fast DCT + 零拷贝 | 592 ms | 1.69x 快 |
| Fast DCT | 965 ms | 1.03x 快 |
| OpenCV | 996 ms | 基准 |

## 安装

### 从 wheel 安装

```bash
pip install turbojpeg_decoder-1.0.0-cp39-cp39-win_amd64.whl
```

## 使用方法

### 基本使用

```python
import turbojpeg_decoder
import numpy as np

# 创建解码器
decoder = turbojpeg_decoder.TurboJpegDecoder()

# 获取图像信息
width, height, channels = decoder.get_image_info("test.jpg")

# 零拷贝解码（推荐）
buffer = np.zeros((height, width, channels), dtype=np.uint8)
decoder.decode_to_buffer("test.jpg", buffer)

# 直接使用 buffer
process_image(buffer)
```

### 替代 OpenCV

```python
# 旧方法（OpenCV）
import cv2
img = cv2.imread("test.jpg")  # 996 ms

# 新方法（TurboJPEG 零拷贝）
import turbojpeg_decoder
decoder = turbojpeg_decoder.TurboJpegDecoder()
width, height, channels = decoder.get_image_info("test.jpg")
buffer = np.zeros((height, width, channels), dtype=np.uint8)
decoder.decode_to_buffer("test.jpg", buffer)  # 578 ms - 快 1.73x!
```

### 视频/连续处理

```python
# 预分配一次 buffer
buffer = np.zeros((height, width, 3), dtype=np.uint8)

# 连续处理
for frame_path in frame_paths:
    decoder.decode_to_buffer(frame_path, buffer)
    process_frame(buffer)

# 性能: 564 ms/张, 1.77 张/秒
```

### 追求极限速度

```python
# Fast DCT 算法（质量损失极小）
img = decoder.decode_fast("test.jpg")  # 965 ms
# 质量损失: max_diff=4，像素差异 > 5 的比例为 0%
```

## API 参考

### `TurboJpegDecoder`

#### `__init__()`
创建解码器实例。

#### `get_image_info(filename)`
获取图像信息。

**参数:**
- `filename` (str): JPEG 文件路径

**返回:**
- `(width, height, channels)`: 图像尺寸和通道数

#### `decode(filename)`
解码 JPEG 图像（标准方法，有内存拷贝）。

**参数:**
- `filename` (str): JPEG 文件路径

**返回:**
- `numpy.ndarray`: 图像数据，形状 `(height, width, channels)`，格式 BGR，类型 uint8

#### `decode_fast(filename)`
解码 JPEG 图像（Fast DCT 算法，速度更快但质量略低）。

**性能:** 比标准方法快约 3%
**质量:** max_diff=4，像素差异 > 5 的比例为 0%

**参数:**
- `filename` (str): JPEG 文件路径

**返回:**
- `numpy.ndarray`: 图像数据

#### `decode_to_buffer(filename, buffer)`
解码 JPEG 图像到预分配的 buffer（零拷贝，推荐方法）。

**性能:** 比标准方法快约 87%，比 OpenCV 快 1.58-1.99x

**参数:**
- `filename` (str): JPEG 文件路径
- `buffer` (numpy.ndarray): 预分配的 array，形状 `(height, width, channels)`
                          数据类型必须是 uint8

**返回:**
- None（结果直接写入 buffer）

## 质量保证

- **零拷贝方法**: 完美匹配（max_diff = 0）
- **Fast DCT**: 极小损失（max_diff = 4）
- **像素差异 > 5**: 0.00%

## 依赖

- Python >= 3.7
- NumPy >= 1.19.0

## 编译

### Windows (Visual Studio)

```bash
pip install pybind11
cd build_turbo
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Linux/macOS

```bash
pip install pybind11
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

## 许可证

MIT License

## 致谢

- [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo) - 高性能 JPEG 库
- [pybind11](https://github.com/pybind/pybind11) - C++/Python 绑定库
