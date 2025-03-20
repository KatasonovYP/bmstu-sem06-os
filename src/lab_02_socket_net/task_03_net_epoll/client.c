#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/times.h>
#include <time.h>

#define OK 0
#define WRITE_OCCUPIED_ERROR 1
#define INVALID_OPERATION_ERROR 2
#define INVALID_FORMAT_ERROR 3
#define OUT_OF_RANGE_ERROR 4

#define PORT 9888

#define READ 0
#define WRITE 1

#define MAX_MESSAGE_SIZE 4096
#define ARRAY_SIZE 26
#define ARRAY_ELEMENT_OCCUPIED '*'
#define ARRAY_ELEMENT_FREE '_'

double get_time()
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

int main(int argc, char **argv)
{
    srand(time(NULL));
    double start_time, end_time;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(PORT);
    sa.sin_addr.s_addr = INADDR_ANY;

    for (;;)
    {
        int socketfd = socket(AF_INET, SOCK_STREAM, 0);
        if (socketfd == -1)
        {
            perror("socket()");
            exit(EXIT_FAILURE);
        }
        if (connect(socketfd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
        {
            perror("connect()");
            exit(EXIT_FAILURE);
        }
        int type = htonl(READ);
        int rc;
        start_time = get_time();
        if (send(socketfd, &type, sizeof(type), 0) == -1)
        {
            perror("write()");
            exit(EXIT_FAILURE);
        }
        int size = recv(socketfd, &rc, sizeof(rc), 0);
        if (size == -1)
        {
            perror("read()");
            exit(EXIT_FAILURE);
        }
        end_time = get_time();
        if (size == 0)
        {
            printf("server close connection\n");
            close(socketfd);
            exit(EXIT_SUCCESS);
        }
        rc = ntohl(rc);

        if (rc != OK)
        {
            printf("Error: %d\n", rc);
            close(socketfd);
            exit(EXIT_FAILURE);
        }
        char arr[ARRAY_SIZE + 1];
        size = recv(socketfd, arr, sizeof(arr), 0);
        if (size == -1)
        {
            perror("read()");
            exit(EXIT_FAILURE);
        }
        arr[size] = '\0';
        if (size == 0)
        {
            printf("server close connection\n");
            close(socketfd);
            exit(EXIT_SUCCESS);
        }

        printf("array: %s\n", arr);

        printf("Read time: %.6f\n", end_time - start_time);

        sleep(1);

        int freeindex = -1;
        for (int i = 0; i < ARRAY_SIZE; i++)
        {
            if (arr[i] != ARRAY_ELEMENT_OCCUPIED)
            {
                freeindex = i;
                break;
            }
        }
        if (freeindex == -1)
        {
            printf("No free elements\n");
            close(socketfd);
            exit(EXIT_FAILURE);
        }

        type = htonl(WRITE);
        start_time = get_time();
        if (send(socketfd, &type, sizeof(type), 0) == -1)
        {
            perror("write()");
            exit(EXIT_FAILURE);
        }
        if (send(socketfd, &freeindex, sizeof(freeindex), 0) == -1)
        {
            perror("write()");
            exit(EXIT_FAILURE);
        }
        end_time = get_time();

        size = recv(socketfd, &rc, sizeof(rc), 0);
        if (size == -1)
        {
            perror("read()");
            exit(EXIT_FAILURE);
        }
        if (size == 0)
        {
            printf("server close connection\n");
            close(socketfd);
            exit(EXIT_SUCCESS);
        }
        rc = ntohl(rc);
        if (rc == WRITE_OCCUPIED_ERROR)
        {
            printf("element %d already occupied\n", freeindex);
        }
        else if (rc != OK)
        {
            printf("Error: %d\n", rc);
            close(socketfd);
            exit(EXIT_FAILURE);
        }
        else
        {
            char result;
            size = recv(socketfd, &result, sizeof(result), 0);
            if (size == -1)
            {
                perror("read()");
                exit(EXIT_FAILURE);
            }
            if (size == 0)
            {
                printf("server close connection\n");
                close(socketfd);
                exit(EXIT_SUCCESS);
            }
            printf("element %d: %c\n", freeindex, result);
        }
        if (close(socketfd) == -1)
        {
            perror("close()");
            exit(EXIT_FAILURE);
        }
        printf("Write time: %.6f\n", end_time - start_time);
        int useconds = rand() % 3000000;
        usleep(useconds);
    }
    exit(EXIT_SUCCESS);
}