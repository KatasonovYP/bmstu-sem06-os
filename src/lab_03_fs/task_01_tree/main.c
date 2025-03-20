#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

#define BUF_SIZE 4096

void display_tree(const char *path, const char *prefix)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char new_prefix[BUF_SIZE];

    if (chdir(path) != 0)
    {
        return;
    }

    if ((dir = opendir(".")) == NULL)
    {
        chdir("..");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            if (lstat(entry->d_name, &st) != -1)
            {
                printf("%s|-- %s\n", prefix, entry->d_name);

                if (S_ISDIR(st.st_mode))
                {
                    snprintf(new_prefix, BUF_SIZE, "%s|   ", prefix);
                    display_tree(entry->d_name, new_prefix);
                }
            }
        }
    }

    closedir(dir);

    chdir("..");
}

int main(int argc, char *argv[])
{
    char original_dir[BUF_SIZE];
    const char *path = (argc > 1) ? argv[1] : ".";

    getcwd(original_dir, BUF_SIZE);

    printf("%s\n", path);

    display_tree(path, "");

    chdir(original_dir);

    return 0;
}
