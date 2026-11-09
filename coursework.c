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

}


int main()
{
    readFile();
    printf("The second txt: \n%s", txt[1].content);//仅作为测试用途，输出第二个读取的文档
    processDocuments();
    buildIndex();
    searchEngine();
    Sleep(20000);
    return 0;
}
