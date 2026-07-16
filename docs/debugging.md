# C-ImageDB 构建、测试与调试

本文面向在仓库根目录工作的维护者。命令均来自当前 `Makefile` 和 `scripts/`；运行集成测试前先阅读数据安全提示。

## 1. 依赖环境

构建要求：

- C11 编译器；`Makefile` 默认 `gcc`，可用 `make CC=clang ...` 覆盖。
- `make`。
- 数学库 `libm`（链接参数 `-lm`）。

完整测试还使用：

- `bash`；
- `python3`；
- `perl`；
- 支持本机 loopback socket 的环境（TCP 测试需要）。

项目不依赖 OpenCV、SQLite 或第三方 C 库。

## 2. 数据安全提示

`tests/run_*.sh`、`tests/search_similar_test.sh`、`tests/report_test.sh`、`tests/verify_repair_test.sh` 和 `scripts/demo.sh` 会删除或重建仓库根目录的 `data/`、`output/`，还会使用 `/tmp`。如果当前 Store 有需要保留的数据，先复制到仓库外再运行集成测试。

`make clean` 只删除目标文件、依赖文件、可执行文件和 C 测试二进制，不删除 Store。

## 3. 构建

推荐入口：

```bash
bash scripts/build.sh debug
bash scripts/build.sh release
bash scripts/build.sh strict
```

- Debug：`-std=c11 -O0 -g3` 加项目警告。
- Release：`-std=c11 -O2 -g` 加项目警告；保留调试符号。
- Strict：Release flags 外加 `-Wconversion -Werror`。

三个 profile 都先执行 `make clean`，再构建 `imagedb`、`cimagedb` 和 `imagedb-server`。

兼容 Make 入口：

```bash
make                 # imagedb + cimagedb
make all server      # 三个可执行文件
make debug
make release
make strict
make help
```

普通构建警告来自 `WARNING_CFLAGS`：`-Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes`。

## 4. 运行

最小 CLI 流程：

```bash
./imagedb help
./imagedb init
./imagedb import samples/sample1.ppm
./imagedb list
./imagedb info 1
./imagedb search 1 3 --metric intersection
```

`imagedb` 和 `cimagedb` 是同一组对象文件链接的两个入口。CLI 的 Store 路径硬编码为当前工作目录下的 `data`，所以从不同工作目录运行会看到不同 Store。

图像处理结果不会自动加入 Store，而是写到命令指定路径：

```bash
mkdir -p output
./imagedb gray 1 output/gray.ppm
./imagedb edge 1 output/edge.bmp
./imagedb export output/metadata.csv
```

TCP 服务：

```bash
./imagedb-server 9090
```

它监听所有本机接口，协议为逐行文本，只支持大写 `LIST`、`INFO <id>`、`SEARCH <id> <k>`、`QUIT`。服务同样使用当前工作目录的 `data`。

## 5. 测试

统一入口：

```bash
bash scripts/test.sh unit
bash scripts/test.sh integration
bash scripts/test.sh all
bash scripts/test.sh benchmark
bash scripts/test.sh sanitizer
```

对应 Make target：

```bash
make test-unit
make test-integration
make test
make benchmark-test
make sanitizer-test
```

`make test-unit` 构建并运行五个二进制：`tests/test_core`、`tests/test_net_io`、`tests/test_store`、`tests/test_cli_parse`、`tests/test_app`。

`make test-integration` 依次运行 CLI、Store、检索、报告、verify/repair 和 TCP 测试。网络测试若在受限容器中因 `bind()` 的 `Operation not permitted` 失败，应在允许绑定 loopback 的环境重跑；不能把环境限制直接判成代码缺陷。

`make sanitizer-test` 用 ASan/UBSan 重新构建并执行完整测试，当前设置 `detect_leaks=0`，所以它不提供完整 leak 检测承诺。

## 6. 使用 LLDB/GDB 调试

先构建 Debug：

```bash
bash scripts/build.sh debug
```

macOS/LLDB 示例：

```bash
lldb -- ./imagedb import samples/sample1.ppm
(lldb) breakpoint set --name cli_run
(lldb) breakpoint set --name app_execute
(lldb) run
(lldb) next
(lldb) frame variable command
(lldb) bt
```

GDB 示例：

```bash
gdb --args ./imagedb search 1 3 --metric l1
(gdb) break app_execute
(gdb) run
(gdb) print *command
(gdb) bt
```

调试会写 Store 的命令时，使用临时工作目录或先备份 `data/`。由于程序使用相对路径 `data`，可以从一个临时目录运行仓库中的绝对路径二进制来隔离 Store。

## 7. 推荐断点

| 场景 | 断点 | 观察内容 |
| --- | --- | --- |
| 参数/退出码 | `cli_run`、`cli_parse`、`fail` | `command.kind`、`cli_parse_error_t`、返回状态 |
| 核心分发 | `app_execute` | `command` union 当前分支、`result->kind` |
| 导入 | `execute_import`、`db_next_id`、`db_commit_import` | hash、ID、目标路径、Record/Feature |
| Store 读取 | `read_array`、`store_file_load_records` | `file_size`、`item_size`、输出重置 |
| Store 成对提交 | `store_file_replace_store` | tmp/bak 标志、`rollback:` 路径 |
| 图像解析 | `ppm_read`、`bmp_read` | header 字段、尺寸、raster 读数 |
| 图像处理 | `execute_image_process`、目标 `process_*` | 源/目标尺寸、owned 指针 |
| 检索 | `search_similar`、`similarity_search_ppm` | metric、候选数量、排序前后结果 |
| CSV 输出失败 | `open_atomic_output`、`finish_atomic_output` | 临时路径、`fflush/fclose/rename` 状态 |
| 图像输出失败 | `cli_output_write_image`、`ppm_write`、`bmp_write` | 目标路径、header/raster 写入和关闭状态 |
| verify/repair | `verify_database`、`repair_database` | summary 各计数、写回条件 |
| TCP framing | `process_bytes`、`process_line` | `line_length`、`discarding`、tokens |
| TCP 短写 | `net_send_all_with` | offset、`errno`、callback 返回值 |

静态函数在未优化的 Debug 构建中通常可直接按名字下断点；若调试器找不到，使用 `breakpoint set --file app.c --name execute_import` 或文件/行断点。

## 8. 检查 Store 文件

只读检查当前布局：

```bash
find data -maxdepth 2 -type f -print
wc -c data/metadata.dat data/features.dat
hexdump -C data/.next_id
```

获取当前 ABI 下结构体大小，可在调试器中：

```text
(lldb) expression -- sizeof(image_record_t)
(lldb) expression -- sizeof(image_feature_t)
```

`metadata.dat`/`features.dat` 是原生结构体数组，不能按固定跨平台 offset 人工编辑。文件大小不是对应 `sizeof(...)` 的整数倍时，`read_array()` 会拒绝。Record 中未终止字符串或指向 Store 外的路径也会被 `records_are_safe()` 拒绝。

`.next_id` 是十进制文本。损坏、空值、非正数或达到 `INT_MAX` 时，`store_file_next_id()` 返回失败且不会分配 ID。

## 9. 常见问题定位

### `Failed to initialize Store`

优先检查当前工作目录、`data/`/`data/images/`/`output/` 权限，以及 `.next_id`、`metadata.dat`、`features.dat` 是否可打开。入口链是 `app_execute(APP_COMMAND_INIT)` → `db_init()` → `store_file_init()`。

### `Failed to read image file`

import 只按 `.bmp` 扩展名选择 BMP；其余文件都尝试 PPM。检查 PPM 是否为 P6、maxval 是否为 255、raster 是否截断；检查 BMP 是否为 24-bit BI_RGB、正高度且 header/像素完整。

### 重复导入被拒绝

`execute_import()` 对解码后的 RGB 像素运行 `hash_pixels()`，再由 `image_is_duplicate()` 比较活动 Record 的 `content_hash`。不同文件容器只要解码像素相同，也可能被视为重复。

### Record not found，但 metadata 中似乎有 ID

`db_find_record_by_id()` 忽略 `deleted != 0` 的 Record。运行 `./imagedb stats` 或 `./imagedb verify` 区分逻辑删除与 Store 损坏。

### CLI 成功执行业务但仍返回 1

检查输出阶段：`print_success()` 可能因 `cli_output_write_*()` 失败返回 1；verify/repair 也会在仍有问题时返回非零。核心 `APP_STATUS_OK` 不保证最终进程码必为 0。

### 输出目标已存在或写失败

Record、histogram 和 search CSV 使用同目录临时文件并在完成后 rename；检查父目录是否存在且可写、目标/临时路径是否可创建。图像输出不同：`cli_output_write_image()` 直接调用 `ppm_write()`/`bmp_write()` 打开目标，失败时目标可能已被截断或只写入一部分，不应把它当成原子替换。

### Store 更新后出现 `.bak`

`store_file_replace_store()` 在回滚自身失败时刻意保留 `.bak` 作为可恢复原件。不要盲目删除；先复制完整 `data/`，比较文件大小，并运行 storage/verify 测试或人工恢复计划。

### TCP 测试超时或断连

服务单线程串行处理客户端，单个客户端读写 timeout 为 10 秒。检查端口占用、测试环境 loopback 权限、命令是否大写且以 LF/CRLF 结尾。`process_bytes()` 遇到 NUL 或超长行会丢弃到下一换行。

## 10. 关键功能验证清单

完成非文档代码修改后，至少按风险选择：

```bash
bash scripts/build.sh release
bash scripts/test.sh unit
bash scripts/test.sh all
git diff --check
git diff --stat
```

针对性验证：

- CLI 参数/文本：`tests/test_cli_parse.c` + `tests/run_cli_compat_tests.sh`。
- 核心用例：`tests/test_app.c`。
- Store/磁盘：`tests/test_store.c` + `tests/run_storage_tests.sh`，尤其旧 v1 fixture。
- 图像格式/算法：`tests/test_core.c` + `tests/run_basic_tests.sh`/`run_image_ops_tests.sh`。
- 检索：`tests/search_similar_test.sh` + basic metric 断言。
- TCP：`tests/test_net_io.c` + `tests/run_net_tests.sh`。
- 内存/未定义行为风险：`bash scripts/test.sh sanitizer`。

## 11. 文档与代码不一致时

`README.md` 的模块目录已经与当前拆分同步；`docs/design.md` 和部分历史学习材料仍含拆分前的职责。调试入口应按当前调用链 `main → cli_run → cli_parse → app_execute`，Store I/O 应从 `database.c → storage/store_file.c` 继续跟踪。
