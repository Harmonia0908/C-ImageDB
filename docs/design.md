# C-ImageDB 设计文档

## 1. 相似度计算方法

### 1.1 RGB 颜色直方图

对每个图像的 R、G、B 三个通道分别统计 256-bin 直方图，记录每个亮度值出现的像素数。同时记录各通道平均值作为辅助特征。

### 1.2 度量方式

支持三种相似度度量：

#### l1 (曼哈顿距离)

```
distance = Σ|R_a[i] - R_b[i]| + Σ|G_a[i] - G_b[i]| + Σ|B_a[i] - B_b[i]|
```

- 直接对原始直方图计数做绝对值差求和
- 距离越小越相似
- 时间复杂度 O(768)

#### l2 (欧氏距离)

```
distance = √(Σ(R_a[i] - R_b[i])² + Σ(G_a[i] - G_b[i])² + Σ(B_a[i] - B_b[i])²)
```

- 对原始直方图计数做平方差求和后开方
- 对大差异更敏感（平方放大）
- 距离越小越相似
- 时间复杂度 O(768)

#### intersection (直方图交，默认)

```
对于每个通道 c ∈ {R, G, B}：
  total_a = Σ hist_a_c[i]    total_b = Σ hist_b_c[i]
  norm_a[i] = hist_a_c[i] / total_a
  norm_b[i] = hist_b_c[i] / total_b
  score_c = Σ min(norm_a[i], norm_b[i])
最终 score = (score_R + score_G + score_B) / 3
```

- 对 R/G/B 三个通道分别用各自总像素数做归一化，计算交集
- 三通道交集结果取平均，使不同分辨率且内容相同的图像 score 接近 1.0
- 分数在 [0.0, 1.0] 之间，越高越相似
- 时间复杂度 O(768)

## 2. 图像几何变换

### 2.1 缩放 — 最近邻插值

```
src_x = ⌊x · src_w / new_w⌋
src_y = ⌊y · src_h / new_h⌋
dst[y][x] = src[src_y][src_x]
```

- 直接取最近像素值，不做加权平均
- 放大时会产生锯齿（马赛克效果），缩小时会丢失细节
- 优点：O(W·H) 时间复杂度，实现极简
- 对 new_w / new_h ≤ 0 或溢出做前置检查

### 2.2 旋转 — 90/180/270 度

**90° 顺时针**：
```
new_w = h, new_h = w
dst[y][x] = src[x][h - 1 - y]
```

**180°**：
```
new_w = w, new_h = h
dst[y][x] = src[h - 1 - y][w - 1 - x]
```

**270° 顺时针**：
```
new_w = h, new_h = w
dst[y][x] = src[w - 1 - x][y]
```

- 90° 和 270° 旋转时宽高交换
- 180° 旋转时宽高不变
- 仅支持 90/180/270 度，其他角度报错
- O(W·H) 时间复杂度，无插值损失

## 3. BMP 图像支持

- 读取：解析 BMP File Header (14B) + BITMAPINFOHEADER (40B)，验证 bit_count=24、compression=BI_RGB
- BGR→RGB 转换：读取时自动交换 R/B 通道
- 行对齐：每行补齐到 4 字节边界
- 行序：自底向上存储，写入时相应处理
- 写入：自动生成合法 BMP 文件头和像素数据

## 4. 图像处理增强算法

### 4.1 直方图均衡化

**基本原理**：
1. 将彩色图像转为灰度图
2. 统计 256 个灰度级的直方图 `hist[i]`
3. 计算累积分布函数 `cdf[i] = cdf[i-1] + hist[i]`
4. 将 CDF 线性映射到 [0, 255]：
   `lut[i] = round((cdf[i] - cdf_min) / (total - cdf_min) * 255)`
5. 每个像素通过 LUT 查表得到均衡化后的灰度值
6. 输出为 R=G=B 的灰度 PPM/BMP

### 4.2 中值滤波

**基本原理**：
- 对每个像素，收集其 kernel_size × kernel_size 邻域内的所有像素值
- 对收集的值排序，取中位数替换原像素
- kernel_size=3 时取 9 个值的中位数，kernel_size=5 时取 25 个值的中位数
- 椒盐噪声去除效果显著，边缘保留优于均值滤波
- 边界处理：只收集有效像素（图像内），不填充

### 4.3 高斯滤波

**3×3 卷积核**：
```
1  2  1
2  4  2  / 16
1  2  1
```
- 中心权重最高（4/16），四邻域次之（2/16），对角最低（1/16）
- 边界处理：类似均值滤波，按有效像素加权求和后除以实际权重和

### 4.4 亮度/对比度调整

**公式**：
```
new = (old - 128) × contrast + 128 + brightness
```
- brightness：整数偏移，正值增亮，负值变暗
- contrast：浮点因子，>1 增强对比度，<1 降低对比度
- 以 128 为灰度中心，乘以 contrast 后加回 128 再加 brightness
- 结果 clamp 到 [0, 255]

### 4.5 双线性插值

**基本原理**：
1. 目标像素 (x, y) 映射到源图像浮点坐标 (sx, sy)
2. 取 sx 周围的整数 x0=floor(sx), x1=ceil(sx)，sy 同理 y0, y1
3. 四个角点 p00, p10, p01, p11 按水平和垂直权重做双线性加权
4. `value = (1-wx)(1-wy)*p00 + wx(1-wy)*p10 + (1-wx)wy*p01 + wx*wy*p11`
5. 边界处理：当新尺寸为 1 时映射到 0；x1/y1 不超过 width-1/height-1

## 5. 数据库文件组织

### 5.1 存储文件

| 文件 | 内容 | 格式 |
|------|------|------|
| `data/metadata.dat` | `image_record_t` 数组 | 二进制顺序存储 |
| `data/features.dat` | `image_feature_t` 数组 | 二进制顺序存储 |
| `data/.next_id` | 下一个可分配 ID | 文本整数 |
| `data/images/` | 导入的图像文件 | PPM/BMP 原始格式 |

### 5.2 逻辑删除与 compact 机制

- `delete` 命令将 `image_record_t.deleted = 1`，不立即删除数据
- 已删除记录对 `list`/`info`/`search`/`find-name`/`query` 不可见
- `stats` 分别统计 active 和 deleted 记录数
- `compact` 永久移除 `deleted=1` 的记录及其关联的 Feature：
  1. 读取 metadata.dat 和 features.dat
  2. 过滤出 `deleted=0` 的记录
  3. 写入 `metadata.tmp` 和 `features.tmp`
  4. `fflush` + `fclose` 确保写入完整
  5. `rename()` 原子替换原文件
  6. 失败时清理临时文件，原文件不受影响

### 5.3 query 命令设计

支持字段和操作符的组合：

| 字段 | 类型 | 支持操作符 |
|------|------|-----------|
| id | 整数 | eq, ne, gt, ge, lt, le |
| name | 字符串 | eq, ne, contains |
| width | 整数 | eq, ne, gt, ge, lt, le |
| height | 整数 | eq, ne, gt, ge, lt, le |
| format | 字符串（派生） | eq, ne |
| size | 整数 | eq, ne, gt, ge, lt, le |

- `format` 字段由 `record.path` 扩展名派生：`.bmp` → BMP，其他 → PPM
- 数值字段使用 `strtol` 解析，非数值报错
- 不合法字段/操作符/组合均报错

### 5.4 CSV export 设计

- 导出未删除记录
- 第一行为表头：`id,name,path,width,height,channels,format,file_size,import_time`
- 包含逗号或引号的字段用双引号包裹
- 时间戳格式化为 `YYYY-MM-DD HH:MM:SS`

### 5.5 数据一致性保护策略

- `compact` 使用临时文件 + `rename` 实现原子替换
- 所有 `fopen`/`fread`/`fwrite` 返回值检查
- `metadata.dat` 和 `features.dat` 加载时校验文件大小对齐
- 损坏文件返回明确错误，不崩溃

## 6. 结果可视化

### 6.1 直方图 CSV 导出 (hist-export)

- 从 `features.dat` 读取指定图像的 `image_feature_t`
- 输出 256 行 CSV，每行对应一个 bin
- 原始模式：`bin, r, g, b`（整数计数）
- 归一化模式 (`--normalized`)：`bin, r_norm, g_norm, b_norm`（0-1 double）
- 归一化分母为各通道总像素数

### 6.2 直方图图像绘制 (hist-image)

- 生成 768×256 RGB 图像
- 左 256 列：R 通道直方图（红色柱）
- 中 256 列：G 通道直方图（绿色柱）
- 右 256 列：B 通道直方图（蓝色柱）
- 每 bin 对应 1 列像素
- 柱高 = `hist[bin] / max_all_channels * 255`，底部对齐
- 背景黑色 (R=G=B=0)，无需文字标注
- 直接操作 `image_t` 像素缓冲区，不依赖第三方库

### 6.3 检索结果 CSV 导出 (search-export)

- 复用 `search_similar()` 获取结果
- CSV 字段：`rank, id, name, metric, value, path`
- `value` 为距离（l1/l2）或相似度分数（intersection）
- 非法 ID / metric / top_k 报错

### 6.4 检索结果拼图 (search-contact)

- 执行相似检索后，将查询图像和 Top-K 结果横向拼接
- 每张图使用 `process_resize_nearest()` 缩放到 128×128
- 输出尺寸 = 128 × (k+1) 宽, 128 高
- 纯像素拼接，无文字叠加
- 结果少于 k 时按实际数量拼接
- 已删除图像不出现在结果中（由 search_similar 保证）
- 尺寸溢出检查：`total_w * 128 * 3 < SIZE_MAX`

### 6.5 设计定位

这些功能属于图像数据库的可视化输出层：
- 不修改 Store 数据
- 将数据库中的结构化数值（直方图、检索结果）转换为人类可读的 CSV 或可查看的图像
- CSV 适合导入其他分析工具
- 图像输出利用系统已有的 PPM/BMP 写入能力，无需新增外部依赖

## 7. 可选 TCP 服务

TCP 查询服务是 optional 模块，不属于主线图像数据库功能；需要通过 `make server` 单独构建，并由 `tests/run_net_tests.sh` 单独测试。
