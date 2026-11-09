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

    FILE *file;
    char folderPath[1000];
    char searchPath[1200];
    char filePath[1500];

    printf("Please enter folder path: ");
    fgets(folderPath, sizeof(folderPath), stdin);

    folderPath[strcspn(folderPath, "\n")] = '\0';

    snprintf(searchPath, sizeof(searchPath), "%s\\*", folderPath);

    WIN32_FIND_DATAA findData;

    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        printf("No TXT files found.\n");
        return 1;
    }

    do {
        
        static int i=0;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }


        snprintf(filePath, sizeof(filePath),
                 "%s\\%s", folderPath, findData.cFileName);

        file = fopen(filePath, "r");

        if (file == NULL) {
            printf("Cannot open file.\n");
            continue;
        }

        char line[1000];
        while (fgets(line, sizeof(line), file) != NULL) {
            strcat(txt[i].content,line);
        }
        strcpy(txt[i].filename, findData.cFileName);

        fclose(file);

        i++;

    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

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

}


int main()
{
    readFile();
    printf("%s", txt[1].content);
    processDocuments();
    buildIndex();
    searchEngine();
    Sleep(20000);
    return 0;
}
