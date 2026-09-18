# LSM-tree 学习笔记

本文总结当前 `lsm_tree.cpp` 已经实现并学习过的内容。该程序是一个教学用途的极简 LSM-tree，用较少的 C++ 代码展示了从内存写入、生成 SSTable，到查询与 Compaction 的基本流程。

## 1. LSM-tree 解决什么问题

LSM-tree（Log-Structured Merge-tree，日志结构合并树）的核心思想是：先把随机写入集中到内存中的有序结构，数据达到一定规模后再顺序写入磁盘。

基本路径如下：

```text
put()
  ↓
MemTable
  ↓ 达到阈值
Flush
  ↓
SSTable 0、SSTable 1……
  ↓ 文件数量增加
Compaction
  ↓
更少、更整洁的 SSTable
```

这种结构用后台整理和一定的读复杂度，换取较好的写入吞吐。

## 2. 当前代码中的主要数据结构

### 2.1 MemTable

```cpp
std::map<std::string, std::string> memtable;
```

MemTable 保存尚未写入磁盘的数据。

选择 `std::map` 的原因：

- key 自动保持有序；
- 相同 key 再次写入时，新 value 会覆盖旧 value；
- Flush 时可以直接按 key 顺序写出 SSTable。

当前 `put()` 的主要逻辑是：

```cpp
memtable[key] = value;
if (memtable.size() >= limit) flush();
```

### 2.2 SSTable 列表

```cpp
std::vector<std::string> sstables;
```

这里保存的是 SSTable 文件路径，而不是文件中的数据。排列顺序为：

```text
最旧的 SSTable → 最新的 SSTable
```

每次 Flush 都会生成一个新文件，并把路径追加到列表末尾。

## 3. SSTable 的存储方式

SSTable 是 Sorted String Table，即按照 key 排序的不可变记录集合。

当前程序使用文本文件保存记录：

```text
"age" "20"
"city" "Shanghai"
"name" "Bob"
```

`writeSSTable()` 的参数叫 `sortedData`，是为了表达一个重要前提：传入的数据已经按 key 排序。

当前传入类型是 `std::map`，因此顺序由容器自动保证，`writeSSTable()` 本身不再执行排序。

写入时使用 `std::quoted`：

```cpp
out << std::quoted(entry.first) << ' '
    << std::quoted(entry.second) << '\n';
```

它让带有空格的 key 或 value 也能被正确写入和读取。

当前 SSTable 具有两个重要特征：

1. 文件内记录按 key 排序。
2. 文件生成后不进行原地修改。

## 4. Flush 过程

`flush()` 将 MemTable 转换为一个新的 SSTable：

1. 如果 MemTable 为空，直接返回。
2. 生成新的 SSTable 文件名。
3. 把 MemTable 中的有序记录写入文件。
4. 将文件路径加入 `sstables`。
5. 清空 MemTable。

```text
MemTable
age  = 20
name = Alice
      ↓ flush
sstable_0.db
"age"  "20"
"name" "Alice"
```

Flush 完成后，数据从仅存在于内存变成存在于磁盘文件。

## 5. 查询过程

`get()` 的查询顺序是：

```text
MemTable
  ↓ 未找到
最新的 SSTable
  ↓ 未找到
更旧的 SSTable
```

必须优先查询新数据。假设：

```text
sstable_0.db：name = Alice
sstable_1.db：name = Bob
```

正确结果应为 `Bob`。因此 SSTable 虽然以“最旧到最新”保存，但查询时使用反向迭代器，从“最新到最旧”查找。

### 5.1 findInSSTable

`findInSSTable()` 顺序读取单个 SSTable：

- 找到目标 key：通过 `foundValue` 返回 value，并返回 `true`；
- 文件打不开：返回 `false`；
- 扫描结束仍未找到：返回 `false`；
- 当前 key 已大于目标 key：提前停止扫描。

可以提前停止，是因为 SSTable 内部已经按 key 排序。

该函数最初使用：

```cpp
std::optional<std::string>
```

为了避免 VS Code 未按 C++17 配置时出现语法提示，后来改成了兼容性更好的形式：

```cpp
bool findInSSTable(..., std::string& foundValue)
```

同时，代码移除了 C++17 的结构化绑定，使当前版本可以使用 C++14 编译。

## 6. Compaction 过程

磁盘上出现多个 SSTable 后，同一个 key 可能拥有多个版本。Compaction 用于合并文件、删除过期版本并减少读取需要检查的文件数量。

当前 `compact()` 的过程如下。

### 6.1 先执行 Flush

```cpp
flush();
```

最新数据可能仍在 MemTable 中。如果不先 Flush，这部分数据就不会参与本次合并。

这是教学代码的简化设计。真实 LSM-tree 通常不会因为一次 Compaction 强制刷新活跃 MemTable，而是让 Immutable MemTable 在后台独立 Flush。

### 6.2 判断是否需要合并

如果 SSTable 数量不超过一个，就不需要合并：

```cpp
if (sstables.size() <= 1) return;
```

### 6.3 从最旧到最新读取

所有 SSTable 按“最旧到最新”的顺序读入 `merged`：

```cpp
merged[key] = value;
```

由于新文件后读取，相同 key 的新值会覆盖旧值。

例如：

```text
sstable_0.db：
age  = 20
name = Alice

sstable_1.db：
city = Shanghai
name = Bob
```

合并结果为：

```text
age  = 20
city = Shanghai
name = Bob
```

`name = Alice` 是过期版本，因此不会出现在新文件中。

### 6.4 写出并安装新 SSTable

合并结果被写入新的有序文件。新文件成功生成后，程序删除旧 SSTable，并把 `sstables` 更新为只包含新文件。

```text
sstable_0.db ─┐
              ├─ Compaction → sstable_2.db
sstable_1.db ─┘
```

先创建新文件、再删除旧文件，比先删除旧文件更安全。但当前实现仍没有 Manifest、临时文件原子重命名和崩溃恢复，因此还不具备生产级原子性。

## 7. Write Amplification

Write Amplification（写放大）表示存储系统实际写入磁盘的数据量，大于用户逻辑写入的数据量。

```text
写放大系数 = 存储系统实际写入量 ÷ 用户逻辑写入量
```

写放大在 Compaction 中产生，是因为 SSTable 不可变。系统不能直接修改旧文件，只能：

1. 读取旧 SSTable；
2. 合并有效记录；
3. 把有效记录重新写入新 SSTable；
4. 删除旧文件。

即使一条记录从未更新，也会在参加 Compaction 时被重新写入。

当前示例包含四次逻辑写入：

```text
name = Alice
age  = 20
name = Bob
city = Shanghai
```

如果暂时认为每条记录大小相同：

- 两次 Flush 共写入 4 条记录；
- Compaction 清除旧的 `name = Alice`，写入 3 条有效记录；
- 总物理写入量为 7 条记录；
- 简化写放大系数约为 `7 ÷ 4 = 1.75`。

`compact()` 开头的 `flush()` 还可能让 MemTable 中的数据刚写成 SSTable，就立即在 Compaction 中再次被写入。

当前代码每次合并所有 SSTable。如果数据库已经很大，而用户只新增少量数据，大量未变化的历史记录也会被重新写入，因此写放大会越来越明显。

## 8. 三种放大

LSM-tree 需要在三个指标之间进行权衡：

| 指标 | 含义 |
| --- | --- |
| 读放大 | 一次查询需要检查多少个文件或数据块 |
| 写放大 | 一份逻辑数据被 Flush 和 Compaction 重写多少次 |
| 空间放大 | 旧版本、删除标记和临时合并文件额外占用多少空间 |

频繁 Compaction 可以减少文件和旧版本，从而降低读放大与空间放大，但通常会提高写放大。

## 9. 当前实现尚未涉及的能力

当前代码已经展示基本结构，但还不是完整的存储引擎，主要缺少：

1. **WAL**：在 MemTable 尚未 Flush 时提供崩溃恢复。
2. **启动恢复和 Manifest**：重新启动后识别已有 SSTable，并避免文件编号从零开始覆盖旧数据。
3. **删除和 Tombstone**：在不可变 SSTable 上表达逻辑删除。
4. **序列号与多版本数据**：准确判断记录版本，并支持快照。
5. **Immutable MemTable 和后台 Flush**：避免前台写入等待磁盘操作。
6. **真正的 SSTable 格式**：数据块、索引块、元数据、Footer、压缩和校验和。
7. **稀疏索引与 Bloom Filter**：减少查询时的文件和数据扫描。
8. **Block Cache**：缓存经常访问的数据块。
9. **范围查询和多路迭代器**：按 key 范围顺序读取。
10. **流式 Compaction**：使用多个有序迭代器归并，避免把全部数据加载进内存。
11. **Level 0、Level 1 等层级结构**：控制文件重叠与数据库规模。
12. **Compaction 策略**：例如 Leveled、Size-Tiered 和 Universal Compaction。
13. **文件切分和 key 范围元数据**：避免生成单个巨大文件。
14. **并发控制**：保证 Flush、Compaction 和查询可以安全并行。
15. **原子安装与错误处理**：保证程序在写文件或 Compaction 中途崩溃后仍能恢复。

## 10. 后续建议

适合在当前代码基础上按以下顺序继续学习：

1. 启动时恢复已有 SSTable，并修复文件编号覆盖问题。
2. 增加 `remove()` 和 Tombstone。
3. 增加 WAL 与重启恢复。
4. 增加 SSTable 的最小、最大 key 元数据。
5. 增加稀疏索引与 Bloom Filter。
6. 把全量内存合并改为流式多路归并。
7. 实现真正的 Level 0 和 Level 1。

其中最优先的是启动恢复：当前程序虽然把数据写入了文件，但新建 `LSMTree` 对象后不会加载旧 SSTable，并且文件编号会重新从零开始，因此暂时不能安全地跨进程持久化数据。

