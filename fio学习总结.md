# WSL 下学习 fio：阶段总结

## 1. 学习环境

本次实验使用：

- Windows 主机
- WSL（Windows Subsystem for Linux）
- Ubuntu Linux
- fio 3.41
- 测试文件：`testfile.dat`

fio 是 Linux 下常用的 I/O 性能测试工具，可以测试磁盘或文件系统的读写性能。测试结果通常包括：

- IOPS：每秒 I/O 操作数
- BW：带宽
- 延迟：完成一次 I/O 所需的时间
- CPU 使用率
- 磁盘利用率

> 注意：在 WSL 中对文件进行测试时，结果可能同时受到 WSL 虚拟磁盘、Windows 文件系统和缓存的影响。因此结果适合学习和相对比较，不能直接当作物理硬盘的官方规格。

---

## 2. 在 Windows 上安装并进入 WSL

### 2.1 安装 WSL

在 PowerShell 中执行：

```powershell
wsl --install
```

如果系统提示重启，重启电脑后继续操作。也可以查看当前已安装的发行版：

```powershell
wsl --list --online
wsl --list --verbose
```

其中：

- `wsl`：调用 WSL 管理命令
- `--install`：安装 WSL 和默认 Linux 发行版
- `--list`：列出发行版
- `--online`：列出可在线安装的发行版
- `--verbose`：显示发行版、状态和 WSL 版本

### 2.2 启动 Ubuntu

在 PowerShell 中执行：

```powershell
wsl.exe -d Ubuntu
```

或者直接执行：

```powershell
wsl
```

命令解释：

- `wsl.exe`：从 Windows 启动 WSL
- `-d Ubuntu`：指定启动 Ubuntu
- `wsl`：启动默认的 Linux 发行版

### 2.3 localhost 代理提示

有时会看到：

```text
wsl: 检测到 localhost 代理配置，但未镜像到 WSL。
NAT 模式下的 WSL 不支持 localhost 代理。
```

这通常是提示信息，不一定会阻止 WSL 启动。它表示 Windows 设置了只监听本机的代理，而 WSL 的 NAT 网络无法直接使用这个 localhost 地址。

如果不需要在 WSL 中联网使用该代理，可以先忽略。若之后执行 `apt update` 或下载软件失败，再单独配置代理。

### 2.4 创建 Linux 用户

第一次启动 Ubuntu 时会要求创建默认 Unix 用户。例如：

```text
Create a default Unix user account:
```

用户名必须：

- 以小写字母或下划线开头
- 只包含小写字母、数字、下划线和短横线
- 不能直接使用纯数字，例如 `1306`

可以使用类似下面的用户名：

``	ext
fio_user
```

创建密码时，Linux 输入密码不会显示字符，这是正常现象。输入完成后按 Enter 即可。

### 2.5 进入 Ubuntu 后看到的提示符

例如：

``	ext
fio_user@computer:~$
```

或者：

``	ext
Durden:/mnt/host/c/Users/21306#
```

提示符最后的字符通常表示：

- `$`：普通用户
- `#`：root 用户

路径中的：

- `~`：当前用户的家目录
- `/mnt/c`：Windows 的 C 盘在 WSL 中的常见挂载路径
- `/mnt/host/c`：某些 WSL 环境对 Windows C 盘使用的挂载路径

---

## 3. Linux 基础命令

### 3.1 查看当前位置

```bash
pwd
```

`pwd` 是 Print Working Directory 的缩写，显示当前所在目录。

### 3.2 列出文件

```bash
ls
ls -lh
```

- `ls`：列出当前目录中的文件和目录
- `-l`：显示详细信息
- `-h`：用易读的单位显示文件大小，例如 KiB、MiB

### 3.3 切换目录

```bash
cd ~/fio-lab
cd ..
cd ~
```

- `cd`：Change Directory，切换目录
- `~`：当前用户的家目录
- `..`：上一级目录

### 3.4 创建目录

```bash
mkdir -p ~/fio-lab
```

- `mkdir`：创建目录
- `-p`：父目录不存在时一起创建；目录已经存在时不报错

### 3.5 删除测试文件

```bash
rm testfile.dat
```

- `rm`：删除文件

删除前要确认当前目录和文件名，避免误删其他文件。fio 测试文件通常可以安全删除，但不要对不熟悉的路径随意使用 `rm -rf`。

### 3.6 安装软件和更新软件包

```bash
sudo apt update
sudo apt install fio
```

含义：

- `sudo`：以管理员权限执行后面的命令
- `apt`：Ubuntu 的软件包管理工具，原意来自 Advanced Package Tool
- `update`：更新软件包列表
- `install`：安装软件
- `fio`：要安装的软件包名称

检查 fio 是否安装成功：

```bash
fio --version
```

---

## 4. fio 命令结构

一个 fio 命令通常由这些部分组成：

```bash
fio --name=任务名 --filename=测试文件 --rw=读写模式 --bs=块大小 --size=测试大小 --ioengine=I/O引擎
```

常见参数：

| 参数 | 含义 |
|---|---|
| `--name` | 为本次任务命名 |
| `--filename` | 指定测试文件或设备 |
| `--rw` | 读写模式，如 `read`、`write`、`randread` |
| `--bs` | 每次 I/O 的块大小 |
| `--size` | 测试文件大小或测试范围 |
| `--ioengine` | I/O 引擎，如 `sync`、`libaio` |
| `--direct=1` | 尽量绕过操作系统缓存 |
| `--time_based` | 按时间运行，而不是处理完固定数据后立即结束 |
| `--runtime=5s` | 运行 5 秒 |
| `--iodepth` | 同时等待处理的 I/O 请求数量 |

### 4.1 IOPS 和 I/O

IOPS 是：

```text
I/O Operations Per Second
```

意思是“每秒 I/O 操作数”。

这里的 `I/O` 是 Input/Output 的缩写，表示输入和输出，不是二选一的字母。

- 读取测试中，IOPS 表示每秒读取次数
- 写入测试中，IOPS 表示每秒写入次数
- 混合测试中，IOPS 是读写操作的总数

例如：

```text
7251 IOPS × 4 KiB ≈ 28.3 MiB/s
```

### 4.2 MiB 和 MB

fio 常同时显示两种单位：

- `MiB`：1024 × 1024 字节
- `MB`：通常按 1000 × 1000 字节计算

因此同一速度显示为 MiB/s 和 MB/s 时，数字会略有不同。

---

## 5. 四次 fio 实验

以下命令默认在测试目录中执行：

```bash
mkdir -p ~/fio-lab
cd ~/fio-lab
```

### 实验 1：同步顺序写入

命令：

```bash
fio --name=lesson1 --filename=testfile.dat --rw=write --bs=1m --size=128m --ioengine=sync
```

参数含义：

- `--rw=write`：顺序写入
- `--bs=1m`：每次写入 1 MiB
- `--size=128m`：总共写入 128 MiB
- `--ioengine=sync`：使用同步 I/O

结果：

| 指标 | 结果 |
|---|---:|
| 错误 | 0 |
| IOPS | 2,285 |
| 带宽 | 2,286 MiB/s |
| 总 I/O | 128 MiB |
| 运行时间 | 56 ms |
| 平均延迟 | 430.52 微秒 |
| 磁盘利用率 | 0.00% |

解读：

- 测试成功，没有 I/O 错误。
- 测试很快完成，只有 56 毫秒。
- 由于使用普通文件和同步写入，操作系统缓存可能影响结果。
- 这次结果更适合学习 fio 输出格式，不宜直接视为物理磁盘持续写入速度。

### 实验 2：直接顺序读取

命令：

```bash
fio --name=lesson2_read --filename=testfile.dat --rw=read --bs=1m --size=128m --ioengine=sync --direct=1 --time_based --runtime=5s
```

新增参数：

- `--direct=1`：尽量绕过缓存
- `--time_based`：按时间运行
- `--runtime=5s`：运行 5 秒

结果：

| 指标 | 结果 |
|---|---:|
| 错误 | 0 |
| IOPS | 2,754 |
| 带宽 | 2,755 MiB/s |
| 总 I/O | 13.5 GiB |
| 运行时间 | 5.001 s |
| 平均延迟 | 361.33 微秒 |
| 99% 延迟 | 807 微秒 |
| 磁盘利用率 | 88.27% |

解读：

- 虽然文件大小只有 128 MiB，但因为设置了 `--time_based`，fio 在 5 秒内重复读取这个范围。
- 因此总读取量达到 13.5 GiB。
- 与实验 1 相比，这次更接近持续 I/O 测试。

### 实验 3：直接随机 4 KiB 读取

命令：

```bash
fio --name=lesson3_randread --filename=testfile.dat --rw=randread --bs=4k --size=128m --ioengine=sync --direct=1 --time_based --runtime=5s
```

参数含义：

- `--rw=randread`：随机读取
- `--bs=4k`：每次读取 4 KiB
- 其他参数与实验 2 类似

结果：

| 指标 | 结果 |
|---|---:|
| 错误 | 0 |
| IOPS | 7,251 |
| 带宽 | 28.3 MiB/s |
| 总 I/O | 142 MiB |
| 运行时间 | 5.001 s |
| 平均延迟 | 137.05 微秒 |
| 99% 延迟 | 367 微秒 |
| 磁盘利用率 | 86.11% |

解读：

- 4 KiB 块很小，所以即使 IOPS 达到 7,251，带宽仍只有 28.3 MiB/s。
- 随机访问需要不断改变位置，通常比顺序访问更难获得高吞吐。
- `issued rwts: total=36265,0,0,0` 表示发出了读取请求，没有写入请求。

### 实验 4：直接顺序 4 KiB 读取

命令：

```bash
fio --name=lesson4_seqread_4k --filename=testfile.dat --rw=read --bs=4k --size=128m --ioengine=sync --direct=1 --time_based --runtime=5s
```

结果：

| 指标 | 结果 |
|---|---:|
| 错误 | 0 |
| IOPS | 11,500 |
| 带宽 | 44.7 MiB/s |
| 总 I/O | 224 MiB |
| 运行时间 | 5.001 s |
| 平均延迟 | 86.84 微秒 |
| 99% 延迟 | 297 微秒 |
| 磁盘利用率 | 75.47% |

解读：

- 顺序 4 KiB 读取的 IOPS 约为 11,500。
- `11,500 × 4 KiB` 约等于 44.7 MiB/s。
- 50% 的请求在约 71 微秒内完成，99% 的请求在约 297 微秒内完成。

---

## 6. 四次实验结果对比

| 实验 | 访问模式 | 块大小 | 方向 | IOPS | 带宽 | 平均延迟 | 总 I/O |
|---|---|---:|---|---:|---:|---:|---:|
| lesson1 | 顺序 | 1 MiB | 写入 | 2,285 | 2,286 MiB/s | 430.52 µs | 128 MiB |
| lesson2 | 顺序 | 1 MiB | 读取 | 2,754 | 2,755 MiB/s | 361.33 µs | 13.5 GiB |
| lesson3 | 随机 | 4 KiB | 读取 | 7,251 | 28.3 MiB/s | 137.05 µs | 142 MiB |
| lesson4 | 顺序 | 4 KiB | 读取 | 11,500 | 44.7 MiB/s | 86.84 µs | 224 MiB |

### 6.1 主要结论

1. 块大小对带宽影响很大。1 MiB 操作可以得到数 GiB/s 级别的带宽，4 KiB 操作的带宽明显更低。
2. 相同 4 KiB 块大小下，顺序读取比随机读取快。
3. lesson4 的 IOPS 约为 lesson3 的 1.6 倍。
4. lesson4 的平均延迟低于 lesson3：86.84 微秒对 137.05 微秒。
5. `--time_based --runtime=5s` 会让 fio 持续运行 5 秒，因此总 I/O 量可能超过测试文件大小。
6. `--direct=1` 可以减少缓存影响，但 WSL 和虚拟磁盘仍可能影响结果。

---

## 7. 阅读 fio 输出时的顺序

拿到一份 fio 输出后，可以按这个顺序阅读：

### 第一步：检查是否成功

```text
err=0
```

如果不是 0，需要先处理错误。

### 第二步：看主结果

```text
read: IOPS=..., BW=..., io=..., run=...
```

写入测试则会显示：

```text
write: IOPS=..., BW=..., io=..., run=...
```

### 第三步：看延迟

重点看：

- `avg`：平均延迟
- `90.00th`：90% 请求以内的延迟
- `99.00th`：99% 请求以内的延迟
- `99.90th`：尾部慢请求情况

### 第四步：看访问深度

```text
IO depths
```

它能说明请求是否真的形成了队列。

### 第五步：看 CPU 和磁盘统计

- CPU 使用率帮助判断是否由 CPU 成为瓶颈
- `util` 表示 fio 看到的设备忙碌程度
- WSL 中的设备名和利用率不一定等同于 Windows 任务管理器中的物理磁盘数据

---

## 8. 下一步学习方向

下一步可以学习 I/O 队列深度。队列深度表示同时等待处理的 I/O 请求数量。

例如：

```bash
fio --name=lesson5_qd16_randread --filename=testfile.dat --rw=randread --bs=4k --size=128m --ioengine=libaio --iodepth=16 --direct=1 --time_based --runtime=5s
```

这里：

- `libaio`：Linux 异步 I/O 引擎
- `iodepth=16`：最多允许 16 个请求同时处于等待状态

下一次可以把它与 `iodepth=1` 的结果比较，观察：

- IOPS 是否提高
- 带宽是否提高
- 平均延迟是否增加
- `IO depths` 中实际使用的深度是多少

测试结束后，可以删除测试文件：

```bash
rm testfile.dat
```



