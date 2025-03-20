#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.122.1"
#define SERVER_PORT 5470
#define BUFFER_SIZE 28
#define SA struct sockaddr

int flag = 1;

void error_log(const char *message)
{
    printf("%s\n", message);
    perror(message);
    exit(EXIT_FAILURE);
}

void sigint_handler()
{
    flag = 0;
    printf("Signal catched\n");
}

int lin_search(const char *buffer, const int bufsize)
{
    for (int i = 0; i < bufsize; i++)
        if (buffer[i] != '_')
            return i;
    return -1;
}

int main(void)
{

    int c_bytes, index, sockfd;
    int mesg[1];
    char buffer[BUFFER_SIZE];
    buffer[BUFFER_SIZE - 1] = '\0';
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        error_log("Socket");
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &(server_addr.sin_addr.s_addr)) == -1)
        error_log("Inet_pton");

    if (signal(SIGINT, sigint_handler) == (void *)-1)
        error_log("Signal");

    if (connect(sockfd, (SA *)&server_addr, sizeof(server_addr)) == -1)
        error_log("Connect");

    mesg[0] = getpid();
    if (send(sockfd, mesg, sizeof(mesg), 0) == -1)
        error_log("Send");
    srand(time(NULL));
    while (flag)
    {
        if ((c_bytes = recv(sockfd, buffer, sizeof(buffer), 0)) == -1)
            error_log("Recv");
        index = lin_search(buffer, BUFFER_SIZE);
        printf("Index:%d buffer:%s\n", index, buffer);
        mesg[0] = index;
        if (send(sockfd, mesg, sizeof(mesg), 0) == -1)
            error_log("Send");

        if ((c_bytes = recv(sockfd, mesg, sizeof(mesg), 0)) == -1)
            error_log("Recv");
        if (mesg[0] == -1)
            printf("Late\n");
        else
            printf("Get: %c index: %d\n", buffer[mesg[0]], mesg[0]);
        int slp = (rand() % 3) + 1;
        sleep(slp);
        if (index >= (BUFFER_SIZE - 3))
            break;
    };
    close(sockfd);
    return 0;
}
