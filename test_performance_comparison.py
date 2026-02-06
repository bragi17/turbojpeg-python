#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
TurboJPEG Decoder vs OpenCV 性能对比测试
测试已安装的 wheel 包
"""

import sys
import os
import time
import numpy as np

# 确保不导入本地 turbojpeg_decoder 文件夹
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir in sys.path:
    sys.path.remove(current_dir)

# 导入已安装的包
import turbojpeg_decoder
import cv2

print("=" * 80)
print("TurboJPEG Decoder (已安装的 wheel 包) vs OpenCV 性能对比")
print("=" * 80)

test_image = "test.jpg"
if not os.path.exists(test_image):
    print(f"Error: 测试图像不存在: {test_image}")
    sys.exit(1)

file_size = os.path.getsize(test_image) / 1024 / 1024
print(f"\n测试图像: {test_image}")
print(f"文件大小: {file_size:.2f} MB")

# 创建解码器
decoder = turbojpeg_decoder.TurboJpegDecoder()

# 获取图像信息
width, height, channels = decoder.get_image_info(test_image)
print(f"图像尺寸: {width} x {height} x {channels}")
print(f"解码后大小: {width * height * channels / 1024 / 1024:.2f} MB")

NUM_RUNS = 5

# ============================================================================
# 预测试: 将 JPEG 解码并保存为 numpy 格式
# ============================================================================
print("=" * 80)
print("预测试: 将 JPEG 解码并保存为 numpy 格式")
print("=" * 80)

# 使用 TurboJPEG 解码
buffer_save = np.zeros((height, width, channels), dtype=np.uint8)
decoder.decode_to_buffer(test_image, buffer_save)

# 保存为 numpy 格式
npy_file = "test.npy"
start = time.time()
np.save(npy_file, buffer_save)
save_time = time.time() - start
print(f"\n保存到 {npy_file}: {save_time*1000:.2f} ms")

# 检查文件大小
npy_size = os.path.getsize(npy_file) / 1024 / 1024
print(f"文件大小: {npy_size:.2f} MB")

# ============================================================================
# 测试 0: np.load() 读取 numpy 格式 (最快参考)
# ============================================================================
print("\n" + "=" * 80)
print("测试 0: np.load() 读取 numpy 格式 (理论上最快)")
print("=" * 80)

times_npy = []
for i in range(NUM_RUNS):
    start = time.time()
    img_npy = np.load(npy_file)
    elapsed = time.time() - start
    times_npy.append(elapsed)
    print(f"  Run {i+1}: {elapsed*1000:.2f} ms")

avg_npy = sum(times_npy) / len(times_npy)
print(f"\n  np.load() 平均: {avg_npy*1000:.2f} ms")
print(f"  图像形状: {img_npy.shape}, 数据类型: {img_npy.dtype}")

# ============================================================================
# 测试 1: OpenCV imread (基准)
# ============================================================================
print("\n" + "=" * 80)
print("测试 1: OpenCV imread (基准)")
print("=" * 80)

times_opencv = []
for i in range(NUM_RUNS):
    start = time.time()
    img_cv = cv2.imread(test_image)
    elapsed = time.time() - start
    times_opencv.append(elapsed)
    print(f"  Run {i+1}: {elapsed*1000:.2f} ms")

avg_opencv = sum(times_opencv) / len(times_opencv)
print(f"\n  OpenCV 平均: {avg_opencv*1000:.2f} ms")
print(f"  图像形状: {img_cv.shape}, 数据类型: {img_cv.dtype}")

# 验证 np.load() 的正确性
diff_npy = np.abs(img_cv.astype(np.int16) - img_npy.astype(np.int16))
print(f"\n  np.load() 正确性验证: max_diff={diff_npy.max()}, mean_diff={diff_npy.mean():.4f}")

# ============================================================================
# 测试 2: TurboJPEG 零拷贝解码 (推荐方法)
# ============================================================================
print("\n" + "=" * 80)
print("测试 2: TurboJPEG 零拷贝解码 (推荐方法)")
print("=" * 80)

buffer = np.zeros((height, width, channels), dtype=np.uint8)
times_zero_copy = []

for i in range(NUM_RUNS):
    start = time.time()
    decoder.decode_to_buffer(test_image, buffer)
    elapsed = time.time() - start
    times_zero_copy.append(elapsed)
    print(f"  Run {i+1}: {elapsed*1000:.2f} ms")

avg_zero_copy = sum(times_zero_copy) / len(times_zero_copy)
print(f"\n  TurboJPEG 零拷贝平均: {avg_zero_copy*1000:.2f} ms")
print(f"  Buffer 形状: {buffer.shape}, 数据类型: {buffer.dtype}")

# 验证正确性
diff = np.abs(img_cv.astype(np.int16) - buffer.astype(np.int16))
print(f"\n  正确性验证: max_diff={diff.max()}, mean_diff={diff.mean():.4f}")

# ============================================================================
# 测试 3: TurboJPEG 标准 decode (有拷贝)
# ============================================================================
print("\n" + "=" * 80)
print("测试 3: TurboJPEG 标准 decode (有拷贝)")
print("=" * 80)

times_standard = []
for i in range(NUM_RUNS):
    start = time.time()
    img_tj = decoder.decode(test_image)
    elapsed = time.time() - start
    times_standard.append(elapsed)
    print(f"  Run {i+1}: {elapsed*1000:.2f} ms")

avg_standard = sum(times_standard) / len(times_standard)
print(f"\n  TurboJPEG 标准 decode 平均: {avg_standard*1000:.2f} ms")
print(f"  图像形状: {img_tj.shape}, 数据类型: {img_tj.dtype}")

# ============================================================================
# 测试 4: TurboJPEG Fast DCT
# ============================================================================
print("\n" + "=" * 80)
print("测试 4: TurboJPEG Fast DCT (快速算法)")
print("=" * 80)

times_fast = []
for i in range(NUM_RUNS):
    start = time.time()
    img_fast = decoder.decode_fast(test_image)
    elapsed = time.time() - start
    times_fast.append(elapsed)
    print(f"  Run {i+1}: {elapsed*1000:.2f} ms")

avg_fast = sum(times_fast) / len(times_fast)
print(f"\n  TurboJPEG Fast DCT 平均: {avg_fast*1000:.2f} ms")

# 质量对比
diff_fast = np.abs(img_cv.astype(np.int16) - img_fast.astype(np.int16))
print(f"\n  质量对比: max_diff={diff_fast.max()}, mean_diff={diff_fast.mean():.4f}")

# ============================================================================
# 测试 5: 零拷贝 + Buffer 复用 (视频流场景)
# ============================================================================
print("\n" + "=" * 80)
print("测试 5: 零拷贝 + Buffer 复用 (连续解码 10 次)")
print("=" * 80)

buffer_reuse = np.zeros((height, width, channels), dtype=np.uint8)
start = time.time()
for i in range(10):
    decoder.decode_to_buffer(test_image, buffer_reuse)
elapsed_reuse = time.time() - start
avg_reuse = elapsed_reuse / 10
print(f"  总时间: {elapsed_reuse*1000:.2f} ms")
print(f"  平均: {avg_reuse*1000:.2f} ms/张")
print(f"  吞吐量: {10 / elapsed_reuse:.2f} 张/秒")

# ============================================================================
# 性能总结
# ============================================================================
print("\n" + "=" * 80)
print("性能总结")
print("=" * 80)

results = [
    ("np.load() (numpy格式)", avg_npy),
    ("TurboJPEG 零拷贝 + 复用", avg_reuse),
    ("TurboJPEG 零拷贝 (单次)", avg_zero_copy),
    ("TurboJPEG Fast DCT", avg_fast),
    ("TurboJPEG 标准 decode", avg_standard),
    ("OpenCV imread", avg_opencv),
]

print("\n所有方法性能排名（越快越好）:")
print("-" * 80)
print(f"{'排名':<6} {'方法':<30} {'耗时 (ms)':<15} {'相对 OpenCV':<15}")
print("-" * 80)

results_sorted = sorted(enumerate(results), key=lambda x: x[1][1])
for rank, (idx, (name, time_ms)) in enumerate(results_sorted, 1):
    speedup = avg_opencv / time_ms
    if speedup > 1:
        speedup_str = f"快 {speedup:.2f}x"
    elif speedup < 1:
        speedup_str = f"慢 {1/speedup:.2f}x"
    else:
        speedup_str = "相同"

    marker = "[1st]" if rank == 1 else "[2nd]" if rank == 2 else "[3rd]" if rank == 3 else "     "
    print(f"{marker} {rank:<4} {name:<30} {time_ms*1000:<15.2f} {speedup_str:<15}")

print("-" * 80)

# ============================================================================
# 最终结论
# ============================================================================
print("\n" + "=" * 80)
print("最终结论")
print("=" * 80)

best_time = results_sorted[0][1][1]
best_name = results_sorted[0][1][0]
best_speedup = avg_opencv / best_time

print(f"\n最佳方法: {best_name}")
print(f"性能: {best_time*1000:.2f} ms")
print(f"相比 OpenCV: 快 {best_speedup:.2f}x")

print(f"""
推荐使用场景:

0. [最快] 预解码 + np.load():
   → 一次性解码后保存为 .npy 格式
   → 首次: 解码 {avg_zero_copy*1000:.0f} ms + 保存 {save_time*1000:.0f} ms
   → 后续: np.load() {avg_npy*1000:.2f} ms (快 {avg_opencv/avg_npy:.2f}x!)
   → 限制: 需要 {npy_size:.0f} MB 存储空间
   → 适用: 固定数据集，重复读取

1. 视频流/连续处理:
   → 使用 零拷贝 + Buffer 复用
   → 性能: {avg_reuse*1000:.2f} ms/张, {10/elapsed_reuse:.2f} 张/秒
   → 优势: 避免重复内存分配

2. 单张图片解码:
   → 使用 零拷贝 decode_to_buffer()
   → 性能: {avg_zero_copy*1000:.2f} ms
   → 优势: 快 {avg_opencv/avg_zero_copy:.2f}x，完美质量

3. 追求极限速度:
   → 使用 Fast DCT decode_fast()
   → 性能: {avg_fast*1000:.2f} ms
   → 质量损失: max_diff={diff_fast.max()}（极小）

4. 简单场景:
   → 使用 标准 decode()
   → 性能: {avg_standard*1000:.2f} ms
   → 优势: 返回新 array，使用简单

5. 备用方案:
   → OpenCV imread()
   → 性能: {avg_opencv*1000:.2f} ms
""")

print("=" * 80)
print("测试完成！")
print("=" * 80)
