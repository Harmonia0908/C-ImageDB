# Demo 指南

## 一键运行

```bash
bash scripts/demo.sh
```

该脚本会执行完整的演示流程：编译、初始化数据库、导入示例图像、运行图像处理、可视化输出和数据库统计。

## 生成的产物

| 文件 | 说明 |
|------|------|
| `output/demo_gray.ppm` | 灰度化结果（加权平均法） |
| `output/demo_edge.ppm` | Sobel 边缘检测结果 |
| `output/demo_hist.ppm` | RGB 直方图可视化图像（768×256） |
| `output/demo_contact.ppm` | 相似检索结果拼图 |
| `output/demo_metadata.csv` | 导出的图像元数据 CSV |

## 查看输出

PPM 和 BMP 文件可使用以下工具查看：

- **macOS**: 预览 (Preview) 或 `open output/demo_gray.ppm`
- **Linux**: GIMP、ImageMagick (`display output/demo_gray.ppm`)、eog 等
- **转换**: 使用 ImageMagick 转换为 PNG/JPEG
  ```bash
  convert output/demo_gray.ppm output/demo_gray.png
  ```
- **在线工具**: 部分在线图像查看器支持 PPM/BMP 格式

## 手动演示步骤

如果想逐步执行，可以按以下顺序运行：

```bash
make clean && make
rm -rf data output && mkdir -p output
./imagedb init
./imagedb import samples/sample1.ppm
./imagedb import samples/sample2.ppm
./imagedb gray 1 output/demo_gray.ppm
./imagedb edge 1 output/demo_edge.ppm
./imagedb hist-image 1 output/demo_hist.ppm
./imagedb search 1 3 --metric intersection
./imagedb search-contact 1 3 output/demo_contact.ppm
./imagedb export output/demo_metadata.csv
./imagedb stats
```
