#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#define C_CH 3

int main()
{
    const int msg_maxl = 4;
    const int buffer_len = 5;
    const char *parent_msg = "aaa";
    const char *child_msg = "bbb";
    int sd[2];

    char buffer[buffer_len];
    buffer[buffer_len] = '\0';
    pid_t chpid;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sd) == -1)
    {
        perror("Cant socketpair");
        exit(1);
    }

    if ((chpid = fork()) == -1)
    {
        perror("Cant fork");
        exit(1);
    }
    if (chpid == 0)
    {
        close(sd[0]);
        printf("Child %d write: %s\n", getpid(), child_msg);
        if (write(sd[1], child_msg, msg_maxl) == -1)
        {
            perror("Cant write");
            exit(1);
        }
        if (read(sd[1], buffer, msg_maxl) == -1)
        {
            perror("Cant read");
            exit(1);
        }
        printf("Child %d recieve: %s\n", getpid(), buffer);
        close(sd[1]);
        return 0;
    }
    else
    {
        close(sd[1]);
        if (read(sd[0], buffer, msg_maxl) == -1)
        {
            perror("Cant read");
            exit(1);
        }
        printf("Parent recieve: %s from child %d\n", buffer, chpid);
        if (write(sd[0], parent_msg, msg_maxl) == -1)
        {
            perror("Cant write");
            exit(1);
        }
        printf("Parent write: %s\n", parent_msg);
        close(sd[0]);
    }

    return 0;
}