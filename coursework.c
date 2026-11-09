#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
struct text {              
    char filename[300];
    char content[200000];
};
struct text txt[6000];     //这个结构体用来存放读取的文本

int readFile()             //这个函数作用：通过输入文件夹的地址，读取文件夹内所有的文本，并保存进txt[]结构体中。
{
    SetConsoleOutputCP(65001);
    FILE *file;
    int txtCount,checkEmpty;
    char folderPath[1000];
    char searchPath[1200];
    char filePath[1500];
    WIN32_FIND_DATAA findData;
    HANDLE hFind;
    while(1)
    {
        txtCount=0;
        checkEmpty=1;
        printf("Please enter folder path: ");
        fgets(folderPath, sizeof(folderPath), stdin);

        folderPath[strcspn(folderPath, "\n")] = '\0';
        if (strlen(folderPath) == 0)
        {
            printf("Folder path cannot be empty.\n\n");
            continue;
        }

        snprintf(searchPath, sizeof(searchPath), "%s\\*", folderPath);


        hFind = FindFirstFileA(searchPath, &findData);

        if (hFind == INVALID_HANDLE_VALUE ) {
            printf("Invalid folder name.\n\n");
            continue;
        }
        int i=0;
        do {

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            
            char *ext = strrchr(findData.cFileName, '.');

            if (ext != NULL && _stricmp(ext, ".txt") != 0)
            {
                continue;
            }

            printf("Found: %s\n", findData.cFileName);
            txtCount++;

            snprintf(filePath, sizeof(filePath),
                    "%s\\%s", folderPath, findData.cFileName);

            file = fopen(filePath, "r");

            if (file == NULL) {
                printf("Cannot open the file.\n");
                continue;
            }

            char line[1000];
            while (fgets(line, sizeof(line), file) != NULL) {
                strcat(txt[i].content,line);
                checkEmpty=0;
            }
            strcpy(txt[i].filename, findData.cFileName);

            fclose(file);

            i++;

        } while (FindNextFileA(hFind, &findData));
        
        FindClose(hFind);

        if(txtCount==0)
        {
            printf("No txt file found!\n\n");
            continue;
        }
        if(checkEmpty==1)
        {
            printf("No txt in the file!\n\n");
            continue;
        }
        break;

    }



    return 0;
}


int processDocuments()     //处理读取的文本，删除不用的符号：逗号，句号，空格，换行符等等，并且全部转换为小写字母
{

}


int buildIndex()          //倒排索引
{

}


int searchEngine()         //搜索
{
    // 临时倒排索引：每个词通过链表记录出现过的文档和词频。
    enum { BUCKETS = 65537, QUERY_SIZE = 1024 };
    struct Posting {
        int docId;
        unsigned frequency;
        struct Posting *next;
    };
    struct Term {
        char *word;
        struct Posting *postings;
        struct Term *next;
    };
    static struct Term *index[BUCKETS];
    unsigned long scores[sizeof txt / sizeof txt[0]];
    int docCount = 0;
    int status = 0;
    char query[QUERY_SIZE];

    while (docCount < (int)(sizeof txt / sizeof txt[0]) &&
           txt[docCount].filename[0] != '\0')
        ++docCount;
    if (docCount == 0) {
        puts("No documents loaded.");
        return 0;
    }

    // 第一轮只建索引。之后每轮处理一次查询，复用相同的分词规则。
    for (int docId = 0; ; ) {
        int indexing = docId < docCount;
        const char *cursor;
        const struct Term *seen[QUERY_SIZE];
        size_t seenCount = 0;

        if (indexing) {
            cursor = txt[docId].content;
        } else {
            printf("\nSearch query (/exit to return): ");
            fflush(stdout);
            if (!fgets(query, sizeof query, stdin))
                break;
            if (!strchr(query, '\n') && !feof(stdin)) {
                int ch = getchar();
                if (ch != '\n' && ch != EOF) {
                    while ((ch = getchar()) != '\n' && ch != EOF)
                        ;
                    puts("Query is too long.");
                    continue;
                }
            }
            query[strcspn(query, "\r\n")] = '\0';
            if (strcmp(query, "/exit") == 0)
                break;
            memset(scores, 0, sizeof scores);
            cursor = query;
        }

        // 英文字母组成词，其他字符作为分隔符，统一转成小写。
        while (*cursor) {
            char word[QUERY_SIZE];
            size_t length = 0;
            unsigned long hash = 0;

            while (*cursor && !isalpha((unsigned char)*cursor))
                ++cursor;
            while (isalpha((unsigned char)*cursor)) {
                if (length < sizeof word)
                    word[length++] = (char)tolower((unsigned char)*cursor);
                ++cursor;
            }
            if (length == 0 || length == sizeof word)
                continue;
            word[length] = '\0';
            for (size_t i = 0; i < length; ++i)
                hash = hash * 31 + (unsigned char)word[i];

            struct Term **slot = &index[hash % BUCKETS];
            while (*slot && strcmp((*slot)->word, word) != 0)
                slot = &(*slot)->next;

            if (indexing) {
                if (!*slot) {
                    struct Term *term = malloc(sizeof *term + length + 1);
                    if (!term)
                        goto allocation_error;
                    term->word = (char *)(term + 1);
                    strcpy(term->word, word);
                    term->postings = NULL;
                    term->next = NULL;
                    *slot = term;
                }

                struct Posting *posting = (*slot)->postings;
                if (posting && posting->docId == docId) {
                    ++posting->frequency;
                } else {
                    struct Posting *entry = malloc(sizeof *entry);
                    if (!entry)
                        goto allocation_error;
                    *entry = (struct Posting){docId, 1, posting};
                    (*slot)->postings = entry;
                }
            } else if (*slot) {
                // 重复的查询词只计一次，分数为所有命中词的词频之和。
                size_t i = 0;
                while (i < seenCount && seen[i] != *slot)
                    ++i;
                if (i < seenCount)
                    continue;
                seen[seenCount++] = *slot;
                for (struct Posting *p = (*slot)->postings; p; p = p->next)
                    scores[p->docId] += p->frequency;
            }
        }

        if (indexing) {
            ++docId;
            continue;
        }
        if (seenCount == 0) {
            puts("No matching documents.");
            continue;
        }

        int topK = 5;
        printf("Top K (Enter for 5): ");
        fflush(stdout);
        if (!fgets(query, sizeof query, stdin))
            break;
        if (!strchr(query, '\n') && !feof(stdin)) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;
            puts("Enter a valid Top K.");
            continue;
        }
        query[strcspn(query, "\r\n")] = '\0';
        if (query[0]) {
            char *end;
            long requested = strtol(query, &end, 10);
            if (*end || requested <= 0) {
                puts("Enter a positive whole number.");
                continue;
            }
            topK = requested > docCount ? docCount : (int)requested;
        }

        // 每次取剩余的最高分，同分时先显示文档编号较小的文件。
        puts("Rank\tFile\tScore");
        for (int rank = 1; rank <= topK; ++rank) {
            int best = -1;
            for (int id = 0; id < docCount; ++id) {
                if (scores[id] > 0 && (best < 0 || scores[id] > scores[best]))
                    best = id;
            }
            if (best < 0)
                break;
            printf("%d\t%s\t%lu\n", rank, txt[best].filename, scores[best]);
            scores[best] = 0;
        }
    }
    status = ferror(stdin) != 0;
    goto cleanup;

allocation_error:
    fputs("Could not allocate the search index.\n", stderr);
    status = 1;

cleanup:
    for (size_t i = 0; i < BUCKETS; ++i) {
        while (index[i]) {
            struct Term *term = index[i];
            index[i] = term->next;
            while (term->postings) {
                struct Posting *entry = term->postings;
                term->postings = entry->next;
                free(entry);
            }
            free(term);
        }
    }
    return status;
}


int main()
{
    readFile();
    processDocuments();
    buildIndex();
    searchEngine();
    return 0;
}
