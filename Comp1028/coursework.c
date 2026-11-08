#include <stdio.h>
#include <windows.h>
int main()
{
    
    SetConsoleOutputCP(65001); //UTF-8
    int num;
    FILE* file[100];
    char content[10000];
    char fileLocation[1000];
    char folderLocation[1000];
    
    printf("Please enter how many files: ");
    if(scanf("%d",&num)==0)
    printf("Invalid input.\n");
    if(num<=0)
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
        while (fgets(content, sizeof(content), file[i]) != NULL)   //Get content here
        {
            printf("%s", content);
        }

    }
    for(int i=0; i<num; i++)
    {
        fclose(file[i]);
    }
    return 0;
}