# C-ImageDB 测试报告

## Phase 4: 检索增强与图像几何变换

### 新增测试项

#### 相似检索增强

| 测试 | 命令 | 验证点 |
|------|------|--------|
| search(intersection) | `search 1 3` | 默认 metric 为 intersection |
| search(l1) | `search 1 3 --metric l1` | l1 距离输出 |
| search(l2) | `search 1 3 --metric l2` | l2 距离输出 |
| search(bad_metric) | `search 1 3 --metric cosine` | 非法 metric 报错 |

#### 图像缩放

| 测试 | 命令 | 验证点 |
|------|------|--------|
| resize | `resize 1 128 128 output/resize_test.ppm` | 输出文件存在 |
| resize(128x128) | Python 解析 | 尺寸确认为 128x128 |
| resize(BMP) | `resize 1 32 32 output/resize_out.bmp` | BMP 输出 |
| resize(0) | `resize 1 0 100 output/x.ppm` | 零尺寸报错 |

#### 图像旋转

| 测试 | 命令 | 验证点 |
|------|------|--------|
| rotate(90) | `rotate 1 90 output/rotate90.ppm` | 输出文件存在 |
| rotate(180) | `rotate 1 180 output/rotate180.ppm` | 输出文件存在 |
| rotate(270) | `rotate 1 270 output/rotate270.ppm` | 输出文件存在 |
| rotate(dims) | Python 解析 | 尺寸正确 |
| rotate(45) | `rotate 1 45 output/x.ppm` | 非法角度报错 |

---

## Phase 5: 数据库增强与查询能力增强

### 新增测试项 (run_basic_tests.sh)

| 测试 | 命令 | 验证点 |
|------|------|--------|
| find-name | `find-name sample1` | 匹配到记录 |
| query | `query width gt 0` | 有结果返回 |
| stats | `stats` | 输出 Total records |
| export | `export output/metadata.csv` | 文件生成 |
| compact | `compact` | 输出 Compact complete |

### 新增测试项 (run_db_tests.sh)

#### find-name 测试

| 测试 | 验证点 |
|------|--------|
| find-name(sample) | 匹配 sample1, sample2 |
| find-name(no-match) | 无匹配输出 No matched |
| find-name(deleted) | 已删除记录不出现 |

#### query 测试

| 测试 | 命令 | 验证点 |
|------|------|--------|
| query(width gt 0) | `query width gt 0` | 有结果 |
| query(format eq PPM) | `query format eq PPM` | 匹配 PPM 记录 |
| query(name contains) | `query name contains sample` | 子串匹配 |
| query(width le) | `query width le 128` | 数值比较 |
| query(bad field) | `query foo eq bar` | 报错 |
| query(bad op) | `query width contains 100` | 报错 |
| query(non-numeric) | `query width eq abc` | 报错 |

#### stats 测试

| 测试 | 验证点 |
|------|--------|
| stats | 非空数据库正常输出 |
| stats(empty) | 空数据库不崩溃 |

#### export 测试

| 测试 | 验证点 |
|------|--------|
| export(file) | CSV 文件生成 |
| export(header) | 表头正确 |
| export(bad path) | 不存在的目录报错 |

#### compact 测试

| 测试 | 验证点 |
|------|--------|
| pre-compact | compact 前 active=3 |
| compact(run) | compact 成功运行 |
| compact(count) | compact 后 active=2 |
| compact(gone) | 已删除 ID 2 不可访问 |
| compact(find-name) | find-name 不再返回已删除记录 |
| compact(search) | search 结果不含已删除记录 |
| compact(query) | query 不匹配已删除记录 |

---

## Phase 6: 图像处理算法增强

### 新增测试项 (run_image_ops_tests.sh)

#### 正常功能测试

| 测试 | 命令 | 验证点 |
|------|------|--------|
| equalize | `equalize 1 output/eq.ppm` | 输出文件存在 |
| equalize(BMP) | `equalize 1 output/eq.bmp` | BMP 输出 |
| median(3) | `median 1 3 output/med3.ppm` | k=3 输出 |
| median(5) | `median 1 5 output/med5.ppm` | k=5 输出 |
| gaussian | `gaussian 1 output/gauss.ppm` | 输出文件存在 |
| adjust | `adjust 1 10 1.2 output/adj.ppm` | 输出文件存在 |
| resize-bilinear | `resize-bilinear 1 128 128 output/bil.ppm` | 输出文件存在 |
| resize-bilinear(BMP) | `resize-bilinear 1 32 64 output/bil.bmp` | BMP 输出 |
| resize-bilinear(128x128) | Python 解析 | 尺寸确认 |

#### 错误处理测试

| 测试 | 命令 | 验证点 |
|------|------|--------|
| median(4) | `median 1 4 output/x.ppm` | 非法 kernel_size 报错 |
| adjust(bad contrast) | `adjust 1 0 0 output/x.ppm` | contrast≤0 报错 |
| adjust(abc) | `adjust 1 0 abc output/x.ppm` | 非数值报错 |
| resize-bilinear(0) | `resize-bilinear 1 0 100 output/x.ppm` | 零尺寸报错 |
| equalize(999) | `equalize 999 output/x.ppm` | 非法 ID 报错 |
| median(999) | `median 999 3 output/x.ppm` | 非法 ID 报错 |
| gaussian(999) | `gaussian 999 output/x.ppm` | 非法 ID 报错 |

### 新增测试项 (run_basic_tests.sh 冒烟测试)

| 测试 | 验证点 |
|------|--------|
| equalize | 输出文件存在 |
| median | k=3 输出文件存在 |
| gaussian | 输出文件存在 |
| adjust | 输出文件存在 |
| resize-bilinear | 输出文件存在 |

---

## Phase 7: 结果可视化与演示增强

### 新增测试项 (run_visual_tests.sh)

#### 正常功能测试

| 测试 | 命令 | 验证点 |
|------|------|--------|
| hist-export | `hist-export 1 output/hist.csv` | CSV 文件存在 |
| hist-export(header) | 检查 CSV 表头 | `bin,r,g,b` |
| hist-export(norm) | `hist-export 1 ... --normalized` | CSV 文件存在 |
| hist-export(norm header) | 检查 CSV 表头 | `r_norm,g_norm,b_norm` |
| hist-image(PPM) | `hist-image 1 output/hist.ppm` | 文件存在 |
| hist-image(768x256) | Python 解析 | 尺寸 768×256 |
| hist-image(BMP) | `hist-image 1 output/hist.bmp` | BMP 文件存在 |
| search-export | `search-export 1 3 output/search.csv` | CSV 文件存在 |
| search-export(header) | 检查 CSV 表头 | `rank,id,name` |
| search-export(l1) | `search-export 1 3 ... --metric l1` | CSV 含 "l1" |
| search-contact(PPM) | `search-contact 1 2 output/contact.ppm` | 文件存在 |
| search-contact(384x128) | Python 解析 | 尺寸 128×(1+2)=384×128 |
| search-contact(BMP) | `search-contact 1 2 output/contact.bmp` | BMP 文件存在 |

#### 错误处理测试

| 测试 | 验证点 |
|------|--------|
| hist-export(999) | 非法 ID 报错 |
| hist-image(999) | 非法 ID 报错 |
| search-export(999) | 非法 ID 报错 |
| search-contact(999) | 非法 ID 报错 |
| search-export(bad metric) | 非法 metric 报错 |
| search-export(k=0) | top_k=0 报错 |
| search-contact(k=0) | top_k=0 报错 |
| hist-export(bad path) | 输出路径不可写报错 |

---

## v1.0-hardening: 收尾修复测试

### 新增 hardening 测试覆盖

| 测试类别 | 覆盖内容 |
|----------|----------|
| PPM 超大数溢出 | width/height 超过 INT_MAX 或 MAX_IMAGE_* 时 import 失败 |
| import 数据一致性 | feature 失败时 import 整体回滚，不残留 orphan record 或 orphan image |
| deleted record 防护 | hist-export/hist-image 对已删除记录必须报错 |
| adjust 参数安全 | brightness 溢出、contrast nan/inf/负值必须报错 |
| search-contact 尺寸溢出 | top_k 过大时安全报错而非生成异常图像 |
| 测试不污染 git status | 不再向 samples/ 写临时文件，git status 保持干净 |

### 测试统计

测试套件覆盖：主线基础测试、数据库测试、图像处理专项测试、可视化输出测试，以及若干 hardening 边界测试。全部套件通过率 100%，编译 Warning 为 0。TCP 网络测试作为 optional 单独维护。
