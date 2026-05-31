# C-ImageDB

[![CI](https://github.com/Harmonia0908/C-ImageDB/actions/workflows/ci.yml/badge.svg)](https://github.com/Harmonia0908/C-ImageDB/actions/workflows/ci.yml)

C-ImageDB 是一个使用 C11 实现的轻量级图像管理、处理与相似检索工具。项目不依赖 OpenCV、SQLite 或第三方图像库，当前支持 PPM P6 和未压缩 24-bit BMP 图像，使用本地二进制文件保存图像元数据和 RGB 直方图特征。

这个项目定位为计算机基础综合实践项目，重点覆盖 C 语言内存管理、文件 I/O、图像格式解析、基础图像处理、简单持久化和命令行测试流程。它不是生产级数据库，也不是语义图像检索系统。

## 功能概览

- PPM P6 图像读写，支持 header 注释，要求 `maxval=255`
- 未压缩 24-bit BMP 读写，处理 BGR/RGB 转换、4 字节行对齐和 bottom-up 行序
- 图像导入到本地 Store，并基于像素内容 hash 做重复导入检测
- 元数据 Record 和 RGB 直方图 Feature 二进制持久化
- 图像处理：灰度化、二值化、均值滤波、Sobel 边缘检测、直方图均衡化、中值滤波、高斯滤波、亮度/对比度调整
- 几何变换：最近邻缩放、双线性缩放、90/180/270 度旋转
- 相似检索：RGB 256-bin 直方图，支持 `intersection`、`l1`、`l2` 三种度量
- Store 管理：列表、详情、逻辑删除、文件名搜索、条件查询、统计、compact、CSV 导出
- 可视化输出：直方图 CSV、直方图图像、检索结果 CSV、检索结果 contact sheet
- 可选 TCP 查询服务：支持 `LIST`、`INFO <id>`、`SEARCH <id> <k>`、`QUIT`
- Shell 集成测试和 GitHub Actions 主线测试

## 当前状态

- 主程序：`./imagedb`
- 兼容/扩展入口：`./cimagedb`，与 `imagedb` 使用同一套 CLI，目前测试脚本主要用它覆盖 `search-similar`、`verify`、`repair`
- 可选 TCP 服务：`./imagedb-server`
- 主线测试：`run_basic_tests.sh`、`run_db_tests.sh`、`run_image_ops_tests.sh`、`run_visual_tests.sh`
- 扩展测试：`search_similar_test.sh`、`report_test.sh`、`benchmark_test.sh`、`verify_repair_test.sh`
- TCP 测试：`run_net_tests.sh`，可选，不在 CI 主线中运行

## 目录结构

```text
C-ImageDB/
  Makefile
  README.md
  CONTEXT.md
  LICENSE
  .github/workflows/ci.yml
  include/
    common.h       通用常量：名称长度、路径长度、最大图像尺寸
    image.h        image_t 内存模型
    ppm.h          PPM P6 读写接口
    bmp.h          BMP 读写接口
    database.h     Store 持久化接口
    process.h      图像处理接口
    feature.h      RGB 直方图特征接口
    search.h       相似检索接口
    similarity.h   外部 PPM 查询图像 Top-K L1 检索接口
    report.h       HTML demo report 生成接口
    verify.h       Store 一致性校验与修复接口
    visualize.h    直方图图像和 contact sheet 接口
    cli.h          CLI 入口接口
    net_server.h   TCP 服务接口
  src/
    main.c         CLI 程序入口
    cli.c          命令解析和命令实现
    image.c        image_t 创建、销毁、复制和校验
    ppm.c          PPM P6 解析和写入
    bmp.c          24-bit BMP 解析和写入
    database.c     metadata/features/.next_id 持久化
    process.c      图像处理算法
    feature.c      RGB 直方图提取和距离/相似度计算
    search.c       Top-K 相似检索
    similarity.c   外部 PPM 查询图像相似检索
    report.c       HTML demo report 生成
    verify.c       Store 校验与修复
    visualize.c    可视化图像生成
    server_main.c  TCP 服务入口
    net_server.c   TCP 命令服务
  scripts/
    generate_samples.sh
    demo.sh
  tests/
    run_basic_tests.sh
    run_db_tests.sh
    run_image_ops_tests.sh
    run_visual_tests.sh
    search_similar_test.sh
    report_test.sh
    benchmark_test.sh
    verify_repair_test.sh
    run_net_tests.sh
  bench/
    benchmark.sh
    results/
    tmp/
  docs/
    design.md
    demo.md
    test_report.md
    release_v1.0.0.md
    adr/0001-content-hash-dedup-on-import.md
  samples/
    sample*.ppm
    sample_bmp*.bmp
  data/            运行时 Store，已被 .gitignore 忽略
  output/          运行时输出目录，已被 .gitignore 忽略
```

## 构建

要求：

- `gcc` 或兼容 C11 的 C 编译器
- `make`
- 测试脚本需要 `bash`、`python3`、`perl`
- 可选 TCP 测试需要 `nc`

构建 CLI：

```bash
make clean
make
```

构建可选 TCP 服务：

```bash
make server
```

生成结果：

- `./imagedb`
- `./cimagedb`
- `./imagedb-server`，仅在执行 `make server` 后生成

## 快速演示

```bash
make clean
make
bash scripts/generate_samples.sh
rm -rf data output
mkdir -p output

./imagedb init
./imagedb import samples/sample1.ppm
./imagedb import samples/sample2.ppm
./imagedb list
./imagedb info 1

./imagedb gray 1 output/gray.ppm
./imagedb edge 1 output/edge.ppm
./imagedb hist-image 1 output/hist.ppm
./imagedb search 1 3 --metric intersection
./imagedb search-export 1 3 output/demo_search.csv --metric l1
./imagedb search-contact 1 3 output/contact.ppm
./imagedb export output/metadata.csv
./imagedb report output output/index.html
./imagedb stats
```

也可以直接运行：

```bash
bash scripts/demo.sh
```

`scripts/demo.sh` 会编译项目、初始化 Store、导入样例图像，并生成：

| 文件 | 说明 |
|---|---|
| `output/demo_gray.ppm` | 灰度化结果 |
| `output/demo_edge.ppm` | Sobel 边缘检测结果 |
| `output/demo_hist.ppm` | RGB 直方图可视化图像，尺寸 768x256 |
| `output/demo_contact.ppm` | 查询图像和检索结果的横向拼图 |
| `output/demo_metadata.csv` | 元数据 CSV |
| `output/demo_search.csv` | Top-K L1 检索结果 CSV |
| `output/index.html` | HTML demo report |

## CLI 命令

### Store

```bash
./imagedb init
./imagedb import <file.ppm|file.bmp>
./imagedb list
./imagedb info <id>
./imagedb delete <id>
./imagedb compact
./imagedb stats
./imagedb export <output.csv>
```

说明：

- `init` 创建 `data/`、`data/images/`、`output/`、`metadata.dat`、`features.dat` 和 `.next_id`
- `import` 会读取图像、计算像素 hash、检查重复、提取 RGB 直方图、复制原图到 `data/images/`
- `delete` 是逻辑删除，只设置 Record 的 `deleted` 标记，不删除 `data/images/` 中的图像文件
- `compact` 会永久移除已逻辑删除的 Record 及其 Feature
- `export` 只导出未删除记录

### 图像处理

```bash
./imagedb gray <id> <out.ppm|out.bmp>
./imagedb binary <id> <threshold 0-255> <out.ppm|out.bmp>
./imagedb blur <id> <out.ppm|out.bmp>
./imagedb edge <id> <out.ppm|out.bmp>
./imagedb equalize <id> <out.ppm|out.bmp>
./imagedb median <id> <3|5> <out.ppm|out.bmp>
./imagedb gaussian <id> <out.ppm|out.bmp>
./imagedb adjust <id> <brightness> <contrast> <out.ppm|out.bmp>
```

实现要点：

- `gray` 使用 `0.299R + 0.587G + 0.114B`
- `binary` 先灰度化，再按阈值输出黑白图
- `blur` 是 3x3 有效邻域均值滤波
- `edge` 是 Sobel 边缘检测，边界像素置 0
- `equalize` 先灰度化，再做灰度直方图均衡化
- `median` 支持 3x3 和 5x5
- `gaussian` 使用固定 3x3 kernel：`1 2 1 / 2 4 2 / 1 2 1`
- `adjust` 使用 `(old - 128) * contrast + 128 + brightness`，结果截断到 `[0,255]`

### 几何变换

```bash
./imagedb resize <id> <new_w> <new_h> <out.ppm|out.bmp>
./imagedb resize-bilinear <id> <new_w> <new_h> <out.ppm|out.bmp>
./imagedb rotate <id> <90|180|270> <out.ppm|out.bmp>
```

- `resize` 使用最近邻插值
- `resize-bilinear` 使用双线性插值
- `rotate` 只支持 90、180、270 度

### 特征与检索

```bash
./imagedb hist <id>
./imagedb search <id> <k>
./imagedb search <id> <k> --metric l1
./imagedb search <id> <k> --metric l2
./imagedb search <id> <k> --metric intersection
./cimagedb search-similar <query.ppm> --topk <k>
```

检索使用导入时保存的 RGB 直方图 Feature。三种 metric：

| Metric | 类型 | 排序方式 | 说明 |
|---|---|---|---|
| `intersection` | 相似度分数 | 越大越相似 | 默认值；对 R/G/B 三通道分别归一化后计算直方图交集，再取平均 |
| `l1` | 距离 | 越小越相似 | 对三通道原始直方图计数做绝对差求和 |
| `l2` | 距离 | 越小越相似 | 对三通道原始直方图计数做平方差求和并开方 |

注意：`l1` 和 `l2` 当前使用原始计数，结果会受图像尺寸影响；`intersection` 做了归一化，更适合不同尺寸图像之间的颜色分布比较。

`search-similar` 与 `search <id> <k>` 不同：它不要求查询图像已经导入 Store，而是读取一个外部 PPM P6 文件，现场提取 RGB 直方图，然后和 Store 中未删除图像的 Feature 做 L1 distance 检索。输出为 CSV 风格文本：

```text
rank,image_path,distance
1,data/images/1.ppm,0.00
2,data/images/2.ppm,8192.00
```

限制：

- 查询文件必须是 PPM P6，不能是 BMP、PNG 或 JPEG
- metric 固定为 L1 distance
- distance 相同时按 Store 记录顺序稳定输出

### 查询与导出

```bash
./imagedb find-name <keyword>
./imagedb query <field> <op> <value>
./imagedb hist-export <id> <output.csv>
./imagedb hist-export <id> <output.csv> --normalized
./imagedb hist-image <id> <out.ppm|out.bmp>
./imagedb search-export <id> <k> <output.csv>
./imagedb search-export <id> <k> <output.csv> --metric l1
./imagedb search-contact <id> <k> <out.ppm|out.bmp>
./imagedb search-contact <id> <k> <out.ppm|out.bmp> --metric l2
./imagedb report <output_dir> <report.html>
./imagedb verify
./imagedb repair
```

`query` 支持字段：

| 字段 | 类型 | 支持操作符 |
|---|---|---|
| `id` | 数值 | `eq`, `ne`, `gt`, `ge`, `lt`, `le` |
| `width` | 数值 | `eq`, `ne`, `gt`, `ge`, `lt`, `le` |
| `height` | 数值 | `eq`, `ne`, `gt`, `ge`, `lt`, `le` |
| `size` | 数值 | `eq`, `ne`, `gt`, `ge`, `lt`, `le` |
| `name` | 字符串 | `eq`, `ne`, `contains` |
| `format` | 字符串 | `eq`, `ne` |

`find-name` 对文件名做不区分大小写的子串匹配；`query name contains` 当前是区分大小写的。

`report` 会读取 demo 产物目录中的 `demo_metadata.csv` 和 `demo_search.csv`，生成 HTML 报告。`scripts/demo.sh` 默认生成 `output/index.html`。报告中直接引用 PPM/BMP 文件，部分浏览器可能无法内联预览 PPM。

`verify` 会检查 Store 中的元数据、图像文件和 Feature 是否一致。`repair` 会尝试删除指向缺失/不可读文件的记录、修复尺寸不一致记录、补生成缺失 Feature，并过滤孤立 Feature。重复 ID 或重复路径这类语义冲突不会自动合并，修复后仍可能通过 `remaining_issues=1` 提示人工处理。

## Store 文件设计

Store 位于 `data/`，由 `init` 创建：

```text
data/
  .next_id        下一个可分配 ID，文本整数
  metadata.dat    image_record_t 数组，二进制顺序存储
  features.dat    image_feature_t 数组，二进制顺序存储
  images/         导入后的 PPM/BMP 文件
```

核心结构：

```c
typedef struct image_record {
    int id;
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    int width;
    int height;
    int channels;
    long file_size;
    long import_time;
    unsigned long content_hash;
    int deleted;
} image_record_t;
```

```c
typedef struct image_feature {
    int image_id;
    int r_hist[256];
    int g_hist[256];
    int b_hist[256];
    double avg_r;
    double avg_g;
    double avg_b;
} image_feature_t;
```

导入提交使用临时文件、备份文件和 `rename()` 尽量保证 `metadata.dat` 与 `features.dat` 一起更新。这里是简化的文件级替换和回滚，不是完整数据库事务；没有 WAL、并发隔离或崩溃恢复。

## Store 校验与修复

校验：

```bash
./cimagedb verify
```

输出示例：

```text
Verify summary:
total_records=2
missing_files=0
missing_histograms=0
duplicate_ids=0
duplicate_paths=0
invalid_records=0
dimension_mismatches=0
metadata_missing=0
feature_store_missing=0
status=OK
```

修复：

```bash
./cimagedb repair
```

修复能力：

- 删除指向不存在文件、无法读取图像或字段非法的未删除 Record
- 按实际图像修复宽、高、通道数字段
- 为缺失 Feature 的 Record 重新提取 RGB 直方图
- 重写 `metadata.dat` 和 `features.dat`，过滤 deleted Record 和孤立 Feature

修复限制：

- 不自动合并重复 ID
- 不自动判断重复 path 哪条记录应保留
- `repair` 结束后会再次 `verify`，如果仍有问题会以非 0 状态退出

## 可选 TCP 服务

构建：

```bash
make server
```

运行：

```bash
./imagedb-server 9002
```

使用 `nc` 测试：

```bash
printf 'LIST\nINFO 1\nSEARCH 1 2\nQUIT\n' | nc -w 2 127.0.0.1 9002
```

支持命令：

- `LIST`
- `INFO <id>`
- `SEARCH <id> <k>`，固定使用 `intersection`
- `QUIT`

限制：

- 单线程、串行处理客户端
- 不支持导入、删除或图像处理
- 无认证、无权限控制、无多客户端并发模型

## 测试

主线测试：

```bash
bash tests/run_basic_tests.sh
bash tests/run_db_tests.sh
bash tests/run_image_ops_tests.sh
bash tests/run_visual_tests.sh
```

扩展测试：

```bash
bash tests/search_similar_test.sh
bash tests/report_test.sh
bash tests/benchmark_test.sh
bash tests/verify_repair_test.sh
```

可选 TCP 测试：

```bash
make server
bash tests/run_net_tests.sh
```

测试覆盖：

| 脚本 | 覆盖内容 |
|---|---|
| `run_basic_tests.sh` | 构建、初始化、导入、PPM/BMP、基础处理、检索、删除、非法参数、坏图像和 hardening 边界 |
| `run_db_tests.sh` | `find-name`、`query`、`stats`、`export`、`compact` |
| `run_image_ops_tests.sh` | `equalize`、`median`、`gaussian`、`adjust`、`resize-bilinear` |
| `run_visual_tests.sh` | `hist-export`、`hist-image`、`search-export`、`search-contact` |
| `search_similar_test.sh` | 外部 PPM 查询图像的 Top-K L1 检索、稳定排序和错误输入 |
| `report_test.sh` | `scripts/demo.sh` 生成 HTML report，以及 report 错误路径处理 |
| `benchmark_test.sh` | `bench/benchmark.sh` 参数校验和结果 CSV |
| `verify_repair_test.sh` | clean verify、缺失图像修复、缺失 Feature 重生成、重复 ID 检测 |
| `run_net_tests.sh` | TCP `LIST/INFO/SEARCH/QUIT`，需要 `nc` |

GitHub Actions 当前运行四组主线测试，不运行 TCP 测试。

## Benchmark

项目包含一个轻量级 shell benchmark：

```bash
bash bench/benchmark.sh
```

默认行为：

- 构建项目
- 生成 100、500、1000 张 16x16 合成 PPM 图像和一张 query 图像
- 临时接管 `data/`，导入不同规模数据集
- 多次测量 `gray`、`edge`、`hist` 和 `search-similar`
- 将结果写入 `bench/results/benchmark.csv`

结果字段：

```text
operation,dataset_size,topk,elapsed_ms
```

可用环境变量调整：

```bash
BENCH_SIZES="100 500" BENCH_REPEATS=3 BENCH_TOPK=10 bash bench/benchmark.sh
```

benchmark 结果受机器性能、磁盘缓存和 shell 启动开销影响，只适合观察趋势，不代表稳定性能承诺。

## 已知限制

- 只支持 PPM P6，且 `maxval` 必须是 255
- 只支持未压缩 24-bit BMP，不支持压缩 BMP、索引色 BMP、32-bit BMP、PNG、JPEG、GIF
- Store 使用结构体二进制直接落盘，跨编译器、跨平台、跨字节序不保证兼容
- 查询和检索基于全量加载和线性扫描，不适合大规模图像库
- 相似检索只基于 RGB 颜色直方图，不理解纹理、形状或语义
- `l1`/`l2` 使用原始直方图计数，容易受图像尺寸影响
- `search-similar` 只支持外部 PPM P6 查询图像，固定使用 L1 distance
- `verify/repair` 只能修复缺失文件、缺失 Feature、尺寸不一致等可机械判断的问题，不能自动合并重复 ID/path
- 没有并发写保护，多进程同时导入、删除或 compact 可能造成数据竞争
- 导入提交是简化的文件级回滚，不是 ACID 事务
- 输出路径由用户指定，程序不会限制只能写到 `output/`
- CSV 导出只做基础转义，不是完整 CSV 库实现
- TCP 服务是可选演示模块，单连接串行，无认证和权限控制

## 后续可改进方向

- 为 Store 写操作增加文件锁，避免多进程写冲突
- 为 Record 建立 ID/名称索引，减少全量扫描
- 检索使用 Top-K 堆，避免对所有候选完整排序
- 对 `l1`/`l2` 使用归一化直方图，降低分辨率影响
- 设计版本化、固定字节序的持久化格式，替代结构体直接落盘
- 补充 C 单元测试、ASan/UBSan、parser fuzz 测试和更系统的性能 benchmark
- 修复 CSV 字段内双引号的严格转义
- 扩展 PNG/JPEG 支持，或明确保持“无第三方依赖”的教学定位
- 将 TCP 服务改造成 `select`/`poll` 多客户端模型

## 简历描述

短版：

> 使用 C11 实现轻量级图像 Store 与相似检索工具，支持 PPM/BMP 导入、基础图像处理、二进制文件持久化、Store 校验修复和 RGB 直方图 Top-K 检索。

详细版：

> 基于纯 C 实现 PPM P6 与未压缩 24-bit BMP 的解析、导入、导出和基础图像处理算法；通过二进制文件持久化 metadata/features，支持逻辑删除、compact、条件查询、CSV 导出、HTML report、Store verify/repair 和 benchmark；基于 RGB 归一化直方图交集、l1、l2 以及外部 PPM 查询图像 L1 distance 实现相似图像检索，并编写 shell 集成测试覆盖非法图像、异常参数、deleted 记录和一致性修复等边界场景。

## License

MIT License. See [LICENSE](LICENSE).
