# C-ImageDB

一个基于 C 语言的图像数据管理与检索系统，支持 PPM 图像导入、元数据持久化、基础图像处理和基于颜色直方图的相似检索。

## Language

**Image**:
内存中的像素矩阵，包含宽度、高度、通道数和像素数据。
_Avoid_: 图像缓冲区, pixel buffer, image buffer

**Record**:
图像的元数据条目，包含 ID、原始文件名、存储路径、尺寸、导入时间等描述信息，不包含像素数据。
_Avoid_: 元数据行, 数据库行, metadata row

**Store**:
Record 和 Feature 的持久化层，通过二进制文件顺序读写实现。不是关系数据库。
_Avoid_: 数据库, database, DB

**Feature**:
从 Image 中提取的 RGB 颜色直方图及其统计量，用于相似检索。
_Avoid_: 特征向量, embedding, descriptor

**Deleted Record**:
被逻辑删除的 Record——`deleted` 标记为真。对 list、search、info 不可见。ID 永不复用。
_Avoid_: 软删除, soft delete, 已移除记录

## Relationships

- 导入一个 **Image** 时，系统创建一个 **Record** 和一个 **Feature**，三者通过 ID 关联
- 导入时基于像素内容哈希去重：若内容哈希与已有 **Record** 匹配，拒绝导入
- 导入先提取 **Feature**，再将 **Record** 与 **Feature** 作为一次逻辑 Store 更新成对提交；普通 I/O 失败会回滚，不保留半份导入
- **Store** 持久化 **Record** 数组和 **Feature** 数组到独立的二进制文件
- **Record** 和 **Feature** 之间存在一一对应关系
- ID 永不复用，单调递增；下一个可用 ID 存储在独立文件 `data/.next_id`

## Example dialogue

> **Dev:** 用户导入了两幅内容完全相同的 PPM 图像，只是文件名不同。Store 中会有几条 Record？
> **Domain expert:** 一条。导入时基于像素内容哈希去重，第二幅会触发"图像已存在"错误，不会被导入。
>
> **Dev:** 用户做了 `delete 1`，然后立刻 `info 1`，会发生什么？
> **Domain expert:** `info 1` 返回错误"记录不存在"。Record 1 已被逻辑删除，对 list/search/info 不可见，但其数据仍保留在 Store 文件中。
>
> **Dev:** 执行 `gray 1 output/gray.ppm` 前，`output/` 目录必须存在吗？
> **Domain expert:** 是的。`init` 命令负责创建整个目录骨架，包括 `output/`。处理命令假定目录已存在，直接写文件。

## Flagged ambiguities

- "数据库" 曾用于指代 Store。已解决：Store 是持久化层，不是数据库系统。避免在代码、文档中使用"数据库"一词。
