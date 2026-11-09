#include <stdio.h>
#include <windows.h>
#include <string.h>
char content[1000000];
int  readFile()   // transfer text to content[]
{
    SetConsoleOutputCP(65001); //UTF-8
    int num;
    FILE* file[1000];
    char fileLocation[1000];
    char line[100000];
    char folderLocation[1000];
    
    printf("Please enter how many files: ");
    if(scanf("%d",&num)==0)
    printf("Invalid input.\n");
    else if(num<=0)
    printf("Invalid input.\n");

    for(int i=0; i<num; i++)
    {
        printf("Please enter the location: ");
        scanf("%s", fileLocation);
        file[i] = fopen(fileLocation, "r");

        if (file[i] == NULL)
        {
            printf("Cannot open the file! Please type it again: \n");
            i--;
            continue;
        }
    }
    for(int i=0; i<num; i++)
    {
        while(fgets(line, sizeof(line), file[i]) != NULL)
        {
            strcat(content, line);
        }
    }
    for(int i=0; i<num; i++)
    {
        fclose(file[i]);
    }
}
//int tokenize()
//{

//}
int main()
{
    
    readFile();
    printf("%s",content);
    //tokenize();
    //
    Sleep(20000);
    return 0;
}
