# C-ImageDB

[![CI](https://github.com/Harmonia0908/C-ImageDB/actions/workflows/ci.yml/badge.svg)](https://github.com/Harmonia0908/C-ImageDB/actions/workflows/ci.yml)

基于 C 语言的图像数据管理与检索系统

A pure C image data management and retrieval system with PPM/BMP support, image processing, histogram-based similarity search, and binary-file persistence.

## 1. 项目简介

C-ImageDB 是一个使用纯 C 语言实现的图像数据管理与检索系统。支持 PPM P6 和未压缩 24-bit BMP 图像的导入导出、元数据持久化、基础数字图像处理、图像几何变换、RGB 颜色直方图特征提取、相似图像检索，以及数据库管理命令和结果可视化输出。

本项目定位为计算机基础综合实践项目，体现 C 语言、文件系统、数据结构、基础图像处理算法和计算机图形学的综合应用。

## Highlights

- 纯 C 实现，不依赖 OpenCV / SQLite / 第三方图像库
- 支持 PPM P6 与未压缩 24-bit BMP 图像导入和输出
- 手写 gray、binary、blur、edge、equalize、median、gaussian、adjust、resize、rotate 等图像处理算法
- 使用二进制文件管理 metadata/features，覆盖基础图像数据库的持久化流程
- 支持逻辑删除、compact、CSV export、find-name、query、stats 等数据库管理命令
- 支持 RGB 归一化直方图交集、l1、l2 三种相似检索度量
- 包含 hardening 测试，覆盖非法图像、超大 header、删除记录、事务一致性和异常参数

## Status

- **Version**: v1.0.0
- Mainline builds and tests are covered by GitHub Actions.
- TCP server is optional and not part of the main test path.

## 2. 功能列表

- **Store 初始化**：创建持久化存储目录和文件
- **图像导入**：导入 PPM P6 与未压缩 24-bit BMP 图像，自动去重
- **图像列表**：列出所有已导入的图像
- **图像详情**：查看指定图像的元数据
- **灰度化**：加权平均法灰度转换
- **二值化**：基于阈值的二值化处理
- **均值滤波**：3x3 邻域均值滤波
- **边缘检测**：Sobel 算子边缘检测
- **颜色直方图**：RGB 三通道 256-bin 直方图提取与展示
- **相似检索**：支持 l1 (曼哈顿)、l2 (欧氏)、intersection (RGB 归一化直方图交集) 三种度量
- **逻辑删除**：软删除图像记录
- **检索结果导出与拼图**：支持 search-export 和 search-contact 可视化输出
- **图像缩放**：最近邻插值缩放
- **图像旋转**：90/180/270 度旋转
- **文件名查找**：按文件名子串搜索（不区分大小写）
- **灵活查询**：支持按 id/name/width/height/format/size 多字段条件查询
- **数据库统计**：记录数、格式分布、平均尺寸、特征数等统计信息
- **数据库压缩**：永久移除已逻辑删除的记录和特征
- **CSV 导出**：将元数据导出为 CSV 格式文件
- **直方图均衡化**：增强图像对比度
- **中值滤波**：3x3/5x5 中值滤波去噪
- **高斯滤波**：3x3 高斯平滑
- **亮度/对比度调整**：线性亮度对比度调节
- **双线性插值缩放**：bilinear interpolation 高质量缩放

## 3. 使用到的知识点

| 知识领域 | 具体应用 |
|----------|----------|
| C 语言 | 结构体、指针、动态内存分配、文件 I/O、函数指针 |
| 数据结构 | 动态数组、结构体序列化、qsort 排序 |
| 数字图像处理 | 灰度化、二值化、均值滤波、Sobel 边缘检测 |
| 计算机图形学 | PPM/BMP 图像格式解析与生成 |
| 数据库基础 | 二进制文件顺序存储、记录管理、ID 分配 |
| 信息检索 | RGB 颜色直方图、曼哈顿距离、Top-K 相似检索 |

## 4. 项目结构

```
C-ImageDB/
  Makefile             构建文件 (make / make server)
  README.md            本文件
  CONTEXT.md           领域术语表
  docs/adr/            架构决策记录
  docs/design.md       设计文档
  docs/release_v1.0.0.md  v1.0.0 发布说明
  docs/test_report.md  测试报告
  include/
    common.h           通用宏定义
    image.h            image_t 类型与内存管理
    ppm.h              PPM 读写接口
    bmp.h              BMP 读写接口
    database.h         Store 持久化接口
    process.h          图像处理接口
    feature.h          特征提取接口
    search.h           相似检索接口
    similarity.h       外部 query.ppm Top-K 相似检索接口
    report.h           HTML demo report 生成接口
    verify.h           数据一致性校验与修复接口
    cli.h              CLI 接口
    net_server.h       TCP 服务接口 (optional)
  src/
    main.c             程序入口
    image.c            图像内存管理实现
    ppm.c              PPM P6 读写实现
    bmp.c              24-bit BMP 读写实现
    database.c         Store 持久化实现
    process.c          图像处理算法实现
    feature.c          特征提取实现
    search.c           相似检索实现
    similarity.c       外部 query.ppm Top-K 相似检索实现
    report.c           HTML demo report 生成实现
    verify.c           数据一致性校验与修复实现
    cli.c              命令行解析与命令实现
    net_server.c       TCP 服务实现 (optional)
    server_main.c      TCP 服务入口 (optional)
  scripts/
    generate_samples.sh  样本图像生成脚本
  tests/
    run_basic_tests.sh   基础测试脚本
    run_db_tests.sh      数据库专项测试
    run_image_ops_tests.sh  图像处理专项测试
    run_visual_tests.sh  可视化输出专项测试
    search_similar_test.sh  外部 query.ppm Top-K 检索测试
    report_test.sh      HTML report 测试
    benchmark_test.sh   benchmark 脚本测试
    verify_repair_test.sh  数据一致性校验与修复测试
    run_net_tests.sh     TCP 服务测试 (optional)
  bench/
    benchmark.sh        图像处理与检索性能基准脚本
    results/            benchmark.csv 输出目录
    tmp/                benchmark 临时文件目录
  samples/             示例 PPM/BMP 图像
  data/                运行时数据目录
  output/              图像处理输出目录
```

## 5. 编译方式

### 主程序

```bash
make clean && make
```

### TCP 服务（可选）

```bash
make server
```

编译要求：
- GCC (或 Clang)
- C11 标准
- `make` 生成 `./imagedb`；`make server` 生成 `./imagedb-server`

## Quick Demo

```bash
make clean && make
rm -rf data output && mkdir -p output
bash scripts/generate_samples.sh
./imagedb init
./imagedb import samples/sample1.ppm
./imagedb import samples/sample2.ppm
./imagedb gray 1 output/gray.ppm
./imagedb edge 1 output/edge.ppm
./imagedb hist-image 1 output/hist.ppm
./imagedb search 1 3 --metric intersection
./imagedb search-export 1 3 output/demo_search.csv --metric l1
./imagedb search-contact 1 3 output/contact.ppm
./imagedb report output output/index.html
./imagedb stats
```

也可以运行 `bash scripts/demo.sh` 一键生成演示输出。

### Demo Outputs

运行 `bash scripts/demo.sh` 后生成：

| 文件 | 说明 |
|------|------|
| `output/demo_gray.ppm` | 灰度化结果 |
| `output/demo_edge.ppm` | Sobel 边缘检测 |
| `output/demo_hist.ppm` | RGB 直方图可视化图像 (768×256) |
| `output/demo_contact.ppm` | 相似检索结果拼图 |
| `output/demo_metadata.csv` | 图像元数据 CSV |
| `output/demo_search.csv` | Top-K L1 检索结果 CSV |
| `output/index.html` | HTML 可视化报告 |

### Demo Report / HTML Visualization

`scripts/demo.sh` 会在生成灰度图、边缘检测图、直方图、metadata CSV 和 Top-K 检索结果后，自动生成 HTML 报告：

```bash
bash scripts/demo.sh
open output/index.html
```

报告位置：

```text
output/index.html
```

报告包含：

- C-ImageDB Demo Report 标题和生成时间
- 输入样例列表
- 灰度图、边缘检测图、RGB 直方图和 Top-K contact sheet
- `demo_metadata.csv` 元数据摘要表
- Top-K 相似检索结果表，包含 rank、image path、distance
- demo 测试输出摘要

报告中的图像直接引用 PPM/BMP 输出文件，不引入 PNG/JPG 转换依赖。部分浏览器可能无法内联预览 PPM 文件，遇到这种情况可以下载查看，或使用支持 PPM 的图片查看器打开对应 `output/*.ppm` 文件。

## 6. 运行示例

```bash
# 生成测试图像
bash scripts/generate_samples.sh

# 初始化 Store
./imagedb init

# 导入图像
./imagedb import samples/sample1.ppm
./imagedb import samples/sample2.ppm

# 查看列表
./imagedb list

# 查看详情
./imagedb info 1

# 图像处理
./imagedb gray 1 output/gray.ppm
./imagedb binary 1 128 output/binary.ppm
./imagedb blur 1 output/blur.ppm
./imagedb edge 1 output/edge.ppm

# 特征与检索
./imagedb hist 1
./imagedb search 1 3
./imagedb search 1 3 --metric l1
./imagedb search 1 3 --metric l2
./cimagedb search-similar samples/sample1.ppm --topk 3

# 图像几何变换
./imagedb resize 1 128 128 output/resized.ppm
./imagedb resize-bilinear 1 128 128 output/resized_bil.ppm
./imagedb rotate 1 90 output/rotated.bmp

# 图像处理增强
./imagedb equalize 1 output/equalized.ppm
./imagedb median 1 3 output/median.ppm
./imagedb gaussian 1 output/gaussian.ppm
./imagedb adjust 1 20 1.2 output/adjusted.ppm

# 数据库管理
./imagedb find-name cat
./imagedb query width gt 256
./imagedb query name contains sample
./imagedb stats
./imagedb compact
./imagedb export output/metadata.csv

# 可视化输出
./imagedb hist-export 1 output/hist.csv
./imagedb hist-export 1 output/hist_norm.csv --normalized
./imagedb hist-image 1 output/histogram.ppm
./imagedb search-export 1 5 output/search_results.csv
./imagedb search-contact 1 5 output/contact_sheet.bmp
```

## 7. 图像处理算法说明

### 灰度化

使用 NTSC 加权公式：`gray = 0.299*R + 0.587*G + 0.114*B`，输出 R=G=B=gray 的图像（支持 PPM/BMP）。

### 二值化

先将图像灰度化，然后使用阈值判断：灰度值 >= threshold 设为 255，否则为 0。threshold 取值范围 0-255。

### 均值滤波

使用 3x3 邻域均值滤波器对每个像素进行平滑处理。边界像素仅使用有效邻域计算均值。

### Sobel 边缘检测

先将图像灰度化，然后使用 Sobel 算子的水平和垂直卷积核计算梯度：

```
Gx = [-1  0  1; -2  0  2; -1  0  1]
Gy = [-1 -2 -1;  0  0  0;  1  2  1]
```

梯度幅值：`mag = |Gx| + |Gy|`，截断到 0-255。边界像素设为 0。

## 8. Store 持久化设计

不使用外部数据库，通过 C 标准库文件 I/O 实现持久化：

- `data/metadata.dat`：顺序存储 `image_record_t` 二进制记录
- `data/features.dat`：顺序存储 `image_feature_t` 二进制记录
- `data/.next_id`：存储下一个可分配的 Image ID
- `data/images/`：存储导入后的 PPM/BMP 图像文件

查询时从文件读取全部记录到内存，通过线性扫描完成查找。适用于小规模数据集。

## 9. 相似检索方法

使用 RGB 颜色直方图作为图像特征：

- 对每个图像的 R、G、B 三个通道分别统计 256-bin 直方图
- 计算平均 R、G、B 值作为辅助特征
- 支持三种相似度度量：
  - **l1 (曼哈顿距离)**：`distance = Σ|hist_a[i] - hist_b[i]|`（距离越小越相似）
  - **l2 (欧氏距离)**：`distance = √(Σ(hist_a[i] - hist_b[i])²)`（距离越小越相似）
  - **intersection (直方图交)**：对 R/G/B 三通道分别做归一化直方图交集 `Σ min(hist_a[i]/total_a, hist_b[i]/total_b)`，再取三通道结果的平均值，score ∈ [0, 1]，分数越高越相似（默认度量）
- 使用 `--metric` 参数选择度量方式，默认为 intersection
- 检索时排除查询图像自身和已逻辑删除的图像
- 对计算出的距离/分数排序，取前 Top-K 个结果

### Similar Image Search

`search-similar` 使用一个外部 PPM P6 文件作为查询图像，对该文件即时计算 RGB 直方图，然后和数据库中已导入图像的直方图做 L1 distance 检索：

```bash
./cimagedb search-similar <query.ppm> --topk K
```

示例：

```bash
./cimagedb search-similar samples/sample1.ppm --topk 3
```

输出示例：

```text
rank,image_path,distance
1,data/images/1.ppm,0.00
2,data/images/2.ppm,8192.00
3,data/images/3.ppm,12288.00
```

- `distance = sum(abs(hist_query[i] - hist_db[i]))`
- distance 越小表示颜色直方图越相似
- 当 `K` 大于数据库中的未删除图像数量时，返回全部结果
- 当 distance 相同，结果按数据库记录顺序稳定输出

## 10. 图像几何变换

### 缩放 (resize)

使用最近邻插值 (nearest neighbor)：
```
src_x = floor(x * src_width / new_width)
src_y = floor(y * src_height / new_height)
```
仅取最近像素值，不做双线性或双三次插值，保证快速且实现简洁。

### 旋转 (rotate)

支持 90、180、270 度旋转：
- **90°**：`dst[y][x] = src[x][h-1-y]`，宽高交换
- **180°**：`dst[y][x] = src[h-1-y][w-1-x]`，宽高不变
- **270°**：`dst[y][x] = src[w-1-x][y]`，宽高交换

### 直方图均衡化 (equalize)

- 先转灰度，计算灰度直方图和累积分布函数 (CDF)
- 将 CDF 映射到 [0,255] 范围，生成查找表
- 每个像素通过查找表映射到新灰度值
- 输出为灰度 PPM/BMP (R=G=B)

### 中值滤波 (median)

- kernel_size 支持 3 或 5
- 对每个像素的邻域内所有像素值排序取中位数
- 边界像素只使用有效邻域
- 彩色图像三通道分别处理

### 高斯滤波 (gaussian)

- 固定 3x3 高斯核：`[1,2,1; 2,4,2; 1,2,1]`
- 边界像素按有效邻域加权归一化
- 彩色图像三通道分别处理

### 亮度/对比度调整 (adjust)

- `new = (old - 128) * contrast + 128 + brightness`
- brightness 为整数偏移量
- contrast 为浮点对比度因子 (> 0)
- 结果 clamp 到 [0, 255]

### 双线性插值缩放 (resize-bilinear)

- 对目标图像每个像素，映射到源图像浮点坐标
- 取周围 4 个最近像素做双线性加权插值
- 比最近邻插值质量更高，无锯齿

## 11. 数据库管理

### find-name — 按文件名查找

```bash
./imagedb find-name <keyword>
```

- 对 record.name 做子串搜索
- **不区分大小写**
- 已删除记录不返回
- 输出：id, name, width, height, format, path

### query — 条件查询

```bash
./imagedb query <field> <op> <value>
```

支持字段：`id`, `name`, `width`, `height`, `format`, `size`

操作符：`eq` (等于), `ne` (不等于), `gt` (大于), `ge` (大于等于), `lt` (小于), `le` (小于等于), `contains` (包含子串)

字段-操作符限制：
- id/width/height/size 支持 eq/ne/gt/ge/lt/le
- name 支持 eq/ne/contains（contains 区分大小写）
- format 支持 eq/ne（值为 PPM 或 BMP）

### stats — 数据库统计

```bash
./imagedb stats
```

输出：
- total_records（含已删除）
- active_records / deleted_records
- total_image_size（未删除图像文件总大小）
- format_count（PPM / BMP 分别统计）
- average_width / average_height
- feature_records（features.dat 中记录数）

### compact — 压缩数据库

```bash
./imagedb compact
```

- 永久移除 `deleted=1` 的记录及其关联的特征记录
- 使用临时文件 + .bak 回滚机制：先备份原文件为 `.bak`，再 `rename` 替换；若中途失败则从 `.bak` 恢复原状
- 输出 compact 前后记录数
- compact 后 `list`/`info`/`search`/`stats` 均正常工作

### export — 导出 CSV

```bash
./imagedb export <output.csv>
```

- 导出未删除记录
- CSV 表头：`id,name,path,width,height,channels,format,file_size,import_time`
- 含逗号或引号的字段自动做 CSV 转义
- 目标目录不存在时报错

### hist-export — 导出直方图 CSV

```bash
./imagedb hist-export <id> <output.csv> [--normalized]
```

- 导出指定图像的 RGB 256-bin 直方图为 CSV
- 默认输出原始计数 (r, g, b)
- `--normalized` 输出归一化值 (r_norm, g_norm, b_norm)，范围 0-1

### hist-image — 绘制直方图图像

```bash
./imagedb hist-image <id> <output.ppm|output.bmp>
```

- 生成 768×256 的 RGB 直方图图像
- 左 256 列为 R 通道，中 256 列为 G 通道，右 256 列为 B 通道
- 柱高按三通道最大 bin 值统一归一化
- 背景黑色，柱状使用对应通道颜色

### search-export — 导出检索结果 CSV

```bash
./imagedb search-export <id> <k> <output.csv> [--metric l1|l2|intersection]
```

- 执行相似检索并将结果导出为 CSV
- CSV 字段：rank, id, name, metric, value, path

### search-contact — 检索结果拼图

```bash
./imagedb search-contact <id> <k> <output.ppm|output.bmp> [--metric ...]
```

- 生成检索结果的横向拼接缩略图
- 第一张为查询图像，后续为 Top-K 结果
- 每张缩放到 128×128，使用最近邻插值
- 输出总尺寸为 128 × (k+1) 宽，128 高

## 12. Data Integrity

C-ImageDB 使用 `data/metadata.dat` 保存元数据记录，使用 `data/features.dat` 保存直方图特征记录。`verify` 和 `repair` 用于检查并修复这些记录与实际图像文件之间的一致性。这里的 histogram 指 `features.dat` 中和图像 id 对应的 RGB 直方图记录。

### verify — 校验数据库一致性

```bash
./cimagedb verify
```

检查内容：

- 元数据存储是否存在并可读取
- feature/histogram 存储是否存在并可读取
- 每条未删除记录指向的图像文件是否存在
- 图像宽高/通道数是否与元数据记录一致
- 每条未删除记录是否有对应直方图特征
- 重复 id / 重复 path
- 空字段或非法字段

输出示例：

```text
Verify summary:
total_records=100
missing_files=2
missing_histograms=5
duplicate_ids=1
duplicate_paths=0
invalid_records=0
dimension_mismatches=0
metadata_missing=0
feature_store_missing=0
status=FAILED
```

### repair — 修复可恢复的问题

```bash
./cimagedb repair
```

修复行为：

- 删除指向不存在文件或无法读取图像的元数据记录
- 重新生成缺失的 histogram/feature 记录
- 按实际图像修复宽高/通道数不一致的记录
- 过滤孤立的 feature 记录

输出示例：

```text
Repair summary:
removed_records=2
regenerated_histograms=5
fixed_dimensions=1
remaining_issues=0
```

`repair` 不会自动猜测如何合并重复 id 或重复 path。遇到这类语义冲突时，修复报告会通过 `remaining_issues=1` 明确提示仍需人工处理。

## 13. Benchmark

项目提供轻量级 shell benchmark，用于评估图像处理命令和 Top-K 相似检索在不同数据规模下的耗时。

运行方式：

```bash
bash bench/benchmark.sh
```

默认测试数据规模为 100、500、1000 张图像，每项操作重复 5 次并输出平均耗时。脚本会自动生成简单 PPM 测试图，不依赖外部数据集；如果当前样本不足，不会崩溃。benchmark 运行期间会临时接管 `data/`，并在结束时恢复原有 `data/`。

结果文件：

```text
bench/results/benchmark.csv
```

CSV 字段：

| 字段 | 含义 |
|------|------|
| `operation` | 操作名称：`grayscale`、`edge`、`histogram`、`search_similar` |
| `dataset_size` | 数据库图像数量 |
| `topk` | Top-K 参数；非检索操作为 0 |
| `elapsed_ms` | 重复运行后的平均耗时，单位毫秒 |

示例结果：

```text
operation,dataset_size,topk,elapsed_ms
histogram,100,0,12
search_similar,100,5,31
search_similar,500,5,144
search_similar,1000,5,298
```

解读方式：

- `grayscale` 和 `edge` 主要反映单张图像处理耗时，和数据库规模关系较小
- `histogram` 当前读取已持久化特征并打印直方图，主要反映特征读取和输出成本
- `search_similar` 会线性扫描数据库特征，耗时通常随 `dataset_size` 近似线性增长
- benchmark 结果受机器性能、磁盘缓存和 shell 启动开销影响，适合比较趋势，不适合作为绝对性能承诺

可用环境变量缩小或调整测试：

```bash
BENCH_SIZES="100 500" BENCH_REPEATS=3 BENCH_TOPK=10 bash bench/benchmark.sh
```

## 14. 测试方法

```bash
# 主线测试
bash tests/run_basic_tests.sh
bash tests/run_db_tests.sh
bash tests/run_image_ops_tests.sh
bash tests/run_visual_tests.sh
bash tests/search_similar_test.sh
bash tests/report_test.sh
bash tests/benchmark_test.sh
bash tests/verify_repair_test.sh

# 可选：TCP 服务测试（需先 make server）
make server
bash tests/run_net_tests.sh
```

GitHub Actions 会在每次 push 和 PR 时自动运行主线四组测试。

## 15. 已知限制

- BMP 仅支持未压缩 24-bit (BI_RGB)，不支持 1/4/8/16/32-bit、RLE 压缩及 JPEG/PNG 内嵌
- 不支持 PNG、JPEG、GIF 等其他图像格式
- PPM 仅支持 P6 (二进制) 格式，maxval=255
- Store 基于全量加载 + 线性扫描，不适合大规模图像库（>10万条记录）
- 特征仅使用 RGB 256-bin 颜色直方图，未实现纹理、形状等高级特征
- 无并发控制，不适用于多进程/多线程场景
- 旋转仅支持 90/180/270 度，不支持任意角度
- `query name contains` 区分大小写（`find-name` 不区分大小写）
- TCP 查询服务为可选模块，需 `make server` 单独构建

## 16. 后续扩展

| 扩展方向 | 涉及知识 |
|----------|----------|
| 哈希表索引加速查询 | 数据结构 |
| Top-K 堆优化检索 | 数据结构 |
| KD 树特征索引 | 数据结构/信息检索 |
| select 多客户端 TCP 服务 | 操作系统/网络 |
| 更多图像格式 (PNG/JPEG) | 图形学/图像压缩 |

## Resume Description

短版：
C 语言实现的轻量图像数据库，支持 PPM/BMP 导入、图像处理、元数据持久化和 RGB 直方图相似检索。

详细版：
使用纯 C 实现 PPM P6 与未压缩 24-bit BMP 的解析、导入、导出和基础图像处理算法。  
通过二进制文件持久化 metadata/features，支持逻辑删除、compact、CSV export、条件查询和数据库统计。  
基于 RGB 归一化直方图交集、l1、l2 实现相似图像检索，并配套覆盖非法图像、边界参数和一致性场景的测试脚本。

## License

MIT License — see [LICENSE](LICENSE) for details.
