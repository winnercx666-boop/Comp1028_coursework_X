# 任务 6：统计与报告

本模块读取已经建立或载入的 `SearchIndex`，输出集合统计、逐文档统计和索引统计。它也能把最近一次查询的结果连同统计导出为 TXT 或 CSV。统计时不重新打开原文档。

## 计数口径

词的识别规则由索引模块统一决定：连续的 ASCII 字母 `A-Z`、`a-z` 构成一个词，转为小写后进入索引；标点、数字和其他非 ASCII 字母字符作为分隔符。超过 1023 字节的完整词与旧代码一样跳过，不把它截成多个词。本项目不删除停用词，所以 `a`、`the` 等词也参与计数。

因此，报告中的“词”是按上述规则得到并被索引接受的词。它不是按空格简单切分的字符串，也不是汉字词语。

| 统计项 | 读取的字段 | 含义 |
| --- | --- | --- |
| 集合文档数 | `doc_count` | 当前集合中的文档数量，已载入的空文档也占一个文档 ID |
| 集合总词数 | `total_terms` | 所有文档中词的出现次数之和，重复出现也计数 |
| 集合唯一词数 | `term_count` | 合并所有文档的词后去重得到的数量 |
| 单文档总词数 | `documents[id].total_terms` | 该文档中词的出现次数 |
| 单文档唯一词数 | `documents[id].unique_terms` | 该文档内去重后的词数 |
| 索引唯一词数 | `term_count` | 索引词表中的词条数量 |
| 倒排关系数 | `posting_count` | 不同的“词—文档”组合数量，一词在一文档中对应一条记录 |

集合唯一词数和索引唯一词数在本项目中相同，因为它们使用同一套分词规则，且索引不删除停用词。集合唯一词数不能通过相加各文档唯一词数得到：不同文档可能包含同一个词。

例如，文档 0 为 `apple apple banana`，文档 1 为 `apple orange`。集合文档数为 2，总词数为 5，唯一词数为 3；每份文档的唯一词数都是 2，但它们相加的 4 不是集合唯一词数。倒排关系数为 4，分别是 `apple—0`、`banana—0`、`apple—1`、`orange—1`。`apple—0` 记录中的出现次数为 2，这个组合仍只算一个关系。

统计模块信任索引维护的计数，不重复遍历倒排表。索引模块负责在建索引和恢复索引时通过 `index_add_posting` 正确更新这些字段。只改变文档集合后，应先重新建立索引再统计；`ready` 为 0 时不会输出可能过期的数字。

## 接口和调用方式

`statistics.h` 声明两个接口：

```c
void statistics_print(const SearchIndex *index, FILE *stream);

int statistics_export(const SearchIndex *index,
                      const SearchResult *results, size_t result_count,
                      const char *query, const char *path, int csv,
                      char *error, size_t error_size);
```

`statistics_print` 把三类统计写入指定流，例如 `stdout`。它不关闭调用者提供的流；未建立或载入索引时会显示提示。

`statistics_export` 的 `csv` 为 0 时输出文本，非 0 时输出 CSV；返回 1 表示完成写入并成功关闭文件，返回 0 表示失败。如果提供了 `error` 和非零的 `error_size`，失败原因会放进这个缓冲区。成功时缓冲区为空。文件会被创建，已有的同名文件会被覆盖；输出路径的父目录必须已经存在。

以下代码展示调用方式；这里的 `index` 应是调用者已经建立或载入的索引对象：

```c
char error[256];

statistics_print(&index, stdout);

if (!statistics_export(&index, NULL, 0, NULL,
                       "report.txt", 0, error, sizeof(error))) {
    fprintf(stderr, "%s\n", error);
}

if (!statistics_export(&index, NULL, 0, NULL,
                       "report.csv", 1, error, sizeof(error))) {
    fprintf(stderr, "%s\n", error);
}
```

如果调用者保存了查询字符串 `query`、结果数组 `results` 和结果数量 `result_count`，可以把它们传入导出函数：

```c
if (!statistics_export(&index, results, result_count, query,
                       "report.csv", 1, error, sizeof(error))) {
    fprintf(stderr, "%s\n", error);
}
```

全部传入 `NULL, 0, NULL` 时只导出统计。提供查询字符串或结果数组时会附上搜索摘要；零条结果也会明确报告数量为 0。模块保留传入结果的顺序和 `score`，不重新排序或执行查询。结果须来自当前索引；重新建立或载入索引后，调用者应清除旧搜索结果。

报告的文档 ID 与索引一致，从 0 开始；搜索排名从 1 开始。

## TXT 与 CSV 内容

两种格式的数字来自相同的索引字段。TXT 使用可读的分节文本；CSV 使用固定的 11 列：

```text
section,document_count,document_id,filename,total_words,unique_words,posting_relationships,query,result_count,rank,score
```

`section` 的值为 `collection`、`document`、`index`，以及可选的 `search`、`search_result`。不适用于该行的字段留空，数量为零的字段写 `0`，这样“没有这个项目”和“项目数量为零”可以区分。空集合仍有集合行和索引行。

CSV 每条记录以 CRLF 结束。字段中包含逗号、双引号、回车或换行时，会用双引号包围整个字段；内部双引号写成两个双引号。例如，文件名 `notes,"draft".txt` 会输出为 `"notes,""draft"".txt"`。包含换行的字段可能占多条物理行，但仍属于一条 CSV 记录，应使用 CSV 解析工具读取。

文件以二进制模式写出，不让 Windows 自动改变 CSV 的换行。名字和查询中的 UTF-8 字节原样保留，不写 BOM，也不做字符编码转换；调用者应提供 UTF-8 字符串。文件路径通过标准 C `fopen` 打开，路径是否支持某种编码取决于运行环境。

## 失败处理与学习说明

导出前会检查索引是否就绪、文档数是否越界、输出路径是否为空，以及结果是否引用了有效的文档 ID。打不开文件、写入失败、关闭时刷出缓冲区失败都会返回 0。写入中途失败时，目标文件可能只包含部分报告；调用者不能把失败返回当成成功导出。

这里用 `size_t` 保存计数，因此 `printf` 使用 `%zu`。倒排记录中的 `docId` 是 `int`，输出该字段时使用 `%d`。文件写入也要检查返回值：一次 `fprintf` 成功不代表缓冲区已经写到磁盘，所以还必须检查 `fclose`。

CSV 的字段转义放在一个公共函数中，所有字段都走同一个规则。这样文件名和查询内容包含特殊字符时，不会因为某一行遗漏引号而破坏整个表格。
