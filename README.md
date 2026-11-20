# COMP1028：文档搜索系统（任务 6、9）

在原小组仓库 `2012ef8` 的分词与词频评分逻辑上，补充统计报告和索引持久化。
以下菜单流程、文件格式和验证记录均对应目录里的代码。

## 先运行

如果从 GitHub 克隆源码，请先按下面“编译”一节生成运行文件；Git 仓库不包含 `.exe`。
代码与运行 ZIP 包则已附编译好的程序。
双击项目根目录的 `run.cmd`，它会在正确目录启动运行文件，并兼容 `build/` 或项目根目录的编译输出。
如果手动运行，请先在项目目录打开终端，再执行：

```powershell
.\build\coursework.exe
```

先用自带小样本确认流程：

1. 输入菜单 `1`，路径输入 `examples/documents`，加载文档并建立索引。
2. 输入菜单 `3`，查看集合、单篇文档和索引统计。
3. 输入菜单 `4`，查询 `apple`，Top K 直接回车；输入 `/exit` 返回菜单。
4. 菜单 `6` 导出 `report.txt`，菜单 `7` 导出 `report.csv`；路径直接回车使用默认名。
5. 菜单 `8` 保存 `index.idx`，路径直接回车。
6. 菜单 `0` 退出，重新打开程序。
7. 直接使用菜单 `9` 加载 `index.idx`。这一步不需要先加载原文档，也不重新分词建立索引。
8. 再用菜单 `3`、`4`、`5` 查看统计、搜索和预览原文。

输出文件相对路径以程序启动时的当前目录为基准。输入路径可包含空格，不要额外加引号。
文档 ID 从 `0` 开始，搜索排名从 `1` 开始。

## 编译

项目已拆为多个 `.c` 和共享 `.h`，请把生产文件一起编译：

```powershell
New-Item -ItemType Directory -Path build -Force
gcc -DENABLE_DIRECTORY_INPUT -std=c17 -O2 -Wall -Wextra -Wpedantic coursework.c collection.c index.c statistics.c persistence.c -o build/coursework.exe
```

`ENABLE_DIRECTORY_INPUT` 保留原仓库的 `dirent.h` 文件夹扫描接口，编译器需提供该头文件。
新的索引、统计和持久化模块仅用标准 C 库。

如果提交环境要求整个构建只依赖标准 C 库，使用配置清单模式：

```powershell
New-Item -ItemType Directory -Path build -Force
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic coursework.c collection.c index.c statistics.c persistence.c -o build/coursework_standard.exe
```

然后运行该程序，菜单 `1` 输入 `examples/collection.txt`。清单每行一个文档路径，
相对路径相对于清单所在目录；空行和以 `#` 开头的注释忽略。两种模式生成的统计、报告和索引文件一致。
可选文件夹适配器单独放在 `collection.c` 的条件编译块内。

安装了 GNU Make 时也可运行 `make`；仅标准 C 模式用 `make CPPFLAGS=`。
Makefile 将程序生成在项目根目录，`run.cmd` 同样支持该位置。

## 你负责的两个功能

| 任务 | 代码 | 功能与菜单 |
|---|---|---|
| 6：统计与报告 | `statistics.c` / `statistics.h` | 菜单 3 查看集合与逐文档统计；菜单 6、7 导出 TXT/CSV，并附上最近展示的搜索结果 |
| 9：持久化索引 | `persistence.c` / `persistence.h` | 菜单 8 保存索引；菜单 9 校验并恢复索引、文档编号、词频、统计和原文 |
| 必要的共享接口 | `search_index.h` / `index.c` | 从原搜索函数中抽出索引、建索引、查询及释放函数，让任务 6 和 9 使用同一份数据 |
| 集成与输入 | `coursework.c` / `collection.c` / `collection.h` | 菜单、文档加载、最近查询管理、可选文件夹扫描及标准 C 清单输入 |

原评分仍为“去重后的查询词在文档中的词频之和”；同分按文档 ID 升序。
词由 ASCII 英文字母组成，统一小写，其他字符为分隔符，没有停用词删除。
超过 1023 字节的连续词按原实现规则跳过。所有统计与搜索使用同一套规则。

统计中“总词数”算每次出现；“不重复词数”算不同词；“倒排关系数”算不同的词-文档组合。
示例文档 `apple apple banana` 和 `apple orange`：总词数 5，不重复词数 3，关系数 4。

索引文件保存实际词条、编号和词频，加载时直接恢复链表，不调用 `index_build()`。
同时保存文档原文，所以原文件暂时不可用时仍能查看文档与生成报告。
读入文件全部通过校验后才替换当前索引，失败则保留原索引。
同名报告/索引输出会被覆盖，输出目录必须已存在。

## 阅读、演示与证据

- `docs/task6-task9-guide.md`：面向初学者的阅读顺序、C 概念、演示和说明视频提纲。
- `docs/statistics.md`：任务 6 的接口、计数口径、CSV 列与错误处理。
- `docs/index-format.md`：任务 9 的完整版本化文件格式、恢复过程和复杂度。
- `docs/validation.md`：真实编译与测试结果、范围和证据位置。
- `docs/ai-usage.md`：这次 AI 辅助工作的记录草稿，方便如实填写课程 AI Usage Log。

小样本只用于理解和检查功能。提交前应与组员在课程指定的 20 Newsgroups / Cranfield 数据及
最终公布的数据上做整个系统的评测，不能把小样本或合成规模检查当作最终 benchmark。
每篇文档最多 199999 字节，集合最多 6000 篇；超过限制或含 NUL 的文件会跳过并提示。
空文档计入文档数量，词数为 0。

合并给组员时，请包含上表的全部生产 `.c` / `.h` 和相关说明，
并更新他们的编译命令。只复制 `statistics.c`、`persistence.c` 无法独立编译。
此实现不调用 AI、检索框架或其他语言来执行程序的核心功能。

## 原仓库介绍

Instructions: Users need to enter the folder path(Example:C:\Users\Desktop\C_project\twenty+newsgroups\20_newsgroups\20_newsgroups\alt.atheism), then this program can automatically load the text file inside the folder.











This is the project of the coursework of Comp 1028




                  Group menbers: Chen Xiang, Liu YiLe, Zhong YiLin, Pan WeiLin.

