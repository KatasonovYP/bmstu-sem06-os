#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 4096
#define SERV_PORT 56441
#define FILLED '_'
#define AMOUNT 50
#define MAX_LINE 4096

int client(int sockfd)
{
    char letters[AMOUNT + 1];
    int local_letters[AMOUNT] = {0};

    ssize_t received = recv(sockfd, letters, MAX_LINE, 0);
    if (received <= 0)
    {
        if (received == 0)
            printf("Server closed connection\n");
        else
            perror("recv");
        return -1;
    }

    letters[received] = '\0'; // Обеспечиваем нулевой символ в конце строки
    printf("Current state received (%ld bytes): %s\n", received, letters);

    // Находим первую незанятую позицию
    int pos = 0;
    while (pos < received && (letters[pos] == FILLED || local_letters[pos] == 1))
        pos++;

    if (pos >= received)
    {
        printf("All positions are occupied or no more positions available, terminating\n");
        return -1;
    }

    char selected_letter = letters[pos];

    local_letters[pos] = 1;

    if (send(sockfd, &pos, sizeof(int), 0) != sizeof(int))
    {
        perror("send");
        return -1;
    }

    printf("Sent position %d to server\n", pos);

    int response_status;
    ssize_t status_received = recv(sockfd, &response_status, sizeof(int), 0);

    if (status_received <= 0)
    {
        if (status_received == 0)
            printf("Server closed connection while waiting for status\n");
        else
            perror("recv status");
        return -1;
    }

    if (response_status == 0)
    {
        printf("SUCCESS: Position %d with letter '%c' was successfully selected!\n",
               pos, selected_letter);
        return 1;
    }
    else
    {
        printf("FAILED: Position %d was already occupied by another client\n", pos);
        return 0;
    }
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr;

    if (argc != 2)
    {
        fprintf(stderr, "usage: client <IPAddress>\n");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(2);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);

    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
    {
        perror("inet_pton");
        exit(3);
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        exit(4);
    }

    printf("Connected to server at %s:%d\n", argv[1], SERV_PORT);

    int attempts = 0;
    int successful_attempts = 0;
    const int max_attempts = 10;

    while (attempts < max_attempts)
    {
        int result = client(sockfd);
        attempts++;

        if (result == 1)
        {
            successful_attempts++;
        }
        else if (result == 0)
        {
            printf("Attempt #%d failed.\n", attempts);
        }
        else
        {
            printf("Error in attempt #%d. Terminating.\n", attempts);
            break;
        }

        if (attempts < max_attempts)
        {
            // printf("Waiting before next attempt (%d/%d completed)...\n",
            //    attempts, max_attempts);
            sleep(2);
        }
    }
    close(sockfd);
    return 0;
}
