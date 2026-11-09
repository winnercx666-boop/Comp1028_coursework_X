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
struct text txt[6000];

int readFile()
{

    FILE *file;
    char folderPath[1000];
    char searchPath[1200];
    char filePath[1500];

    printf("Please enter folder path: ");
    fgets(folderPath, sizeof(folderPath), stdin);

    folderPath[strcspn(folderPath, "\n")] = '\0';


    snprintf(searchPath, sizeof(searchPath),
             "%s\\*", folderPath);

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

        // 8. Close file
        fclose(file);

        i++;

    } while (FindNextFileA(hFind, &findData));

    // 9. Release Windows search handle
    FindClose(hFind);

    return 0;
}
//int processDocuments()
//{

//}
//int buildIndex()
//{

//}

//int searchEngine()
//{

//}
int main()
{
    
    readFile();
    printf("%s", txt[1].content);
    //processDocuments();
    //buildIndex();
    //searchEngine();
    Sleep(20000);
    return 0;
}//"C:\Users\winner\Desktop\C_project\twenty+newsgroups\20_newsgroups\20_newsgroups\alt.atheism"
