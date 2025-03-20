#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

#define SERVER_PORT 56441
#define COUNT 50
#define MAX_LINE 4096

#define ACTIVE_READERS 0
#define WRITE_QUEUE 1
#define READ_QUEUE 2
#define ACTIVE_WRITER 3

#define FILLER '_'

#define P -1
#define V 1

int listen_fd;
int flag = 0;

void sig_handler(int sig_num)
{
    close(listen_fd);
    flag = 1;
    exit(0);
}

void sig_chld(int signo)
{
    pid_t pid;
    int stat;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("child %d terminated\n", pid);
    return;
}

struct sembuf start_read[] = {
    {READ_QUEUE, V, 0},
    {ACTIVE_WRITER, 0, 0},
    {WRITE_QUEUE, 0, 0},
    {ACTIVE_READERS, V, 0},
    {READ_QUEUE, P, 0},
};
struct sembuf stop_read[] = {{ACTIVE_READERS, P, 0}};
struct sembuf start_write[] = {
    {WRITE_QUEUE, V, 0},
    {ACTIVE_READERS, 0, 0},
    {ACTIVE_WRITER, 0, 0},
    {ACTIVE_WRITER, V, 0},
    {WRITE_QUEUE, P, 0},
};
struct sembuf stop_write[] = {{ACTIVE_WRITER, P, 0}};

int init_monitor(int sem_id)
{
    if (semctl(sem_id, ACTIVE_READERS, SETVAL, 0) == -1)
        return EXIT_FAILURE;
    if (semctl(sem_id, WRITE_QUEUE, SETVAL, 0) == -1)
        return EXIT_FAILURE;
    if (semctl(sem_id, READ_QUEUE, SETVAL, 0) == -1)
        return EXIT_FAILURE;
    if (semctl(sem_id, ACTIVE_WRITER, SETVAL, 0) == -1)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

void process_client(int sem_id, char *letters, int connection_fd)
{
    while (1)
    {
        char send_line[MAX_LINE];
        int pos_chosen;
        int response_status;

        if (semop(sem_id, start_read, 5) == -1)
        {
            perror("semop start_read");
            exit(EXIT_FAILURE);
        }
        sprintf(send_line, "%s", letters);
        if (send(connection_fd, send_line, strlen(send_line), 0) != strlen(send_line))
        {
            perror("send");
            exit(EXIT_FAILURE);
        }
        if (semop(sem_id, stop_read, 1) == -1)
        {
            perror("semop stop_read");
            exit(EXIT_FAILURE);
        }

        ssize_t n = recv(connection_fd, &pos_chosen, sizeof(int), 0);
        if (n <= 0)
        {
            if (n == 0)
            {
                printf("Client closed the connection.\n");
            }
            else
            {
                perror("read chosen");
            }
            exit(EXIT_SUCCESS);
        }

        if (semop(sem_id, start_write, 5) == -1)
        {
            perror("semop start_write");
            exit(EXIT_FAILURE);
        }

        if (letters[pos_chosen] == FILLER)
        {
            printf("[WARN] client with pid %d is late with %d\n", getpid(), pos_chosen);
            response_status = 1;
        }
        else
        {
            printf("[SUCCESS] client with pid %d selected '%c' with %d\n", getpid(), letters[pos_chosen], pos_chosen);
            letters[pos_chosen] = FILLER;
            response_status = 0;
        }

        if (send(connection_fd, &response_status, sizeof(int), 0) != sizeof(int))
        {
            perror("send status");
            exit(EXIT_FAILURE);
        }

        if (pos_chosen >= COUNT - 1)
            exit(EXIT_SUCCESS);

        if (semop(sem_id, stop_write, 1) == -1)
        {
            perror("semop stop_write");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv)
{
    int shm_id;
    int sem_id;
    char *letters;
    int connection_fd;
    pid_t child_pid;
    socklen_t client_length;
    struct sockaddr_in client_address, server_address;
    const int permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    key_t sem_key, shm_key;

    if (signal(SIGINT, sig_handler) < 0)
    {
        perror("signal");
        exit(1);
    }
    if (signal(SIGCHLD, sig_chld) < 0)
    {
        perror("signal");
        exit(1);
    }

    if ((sem_key = ftok("./key.txt", 1)) == -1)
    {
        perror("sem_key");
        exit(4);
    }

    if ((sem_id = semget(sem_key, 5, IPC_CREAT | permissions)) == -1)
    {
        perror("sem_id");
        exit(5);
    }

    if ((shm_key = ftok("./key.txt", 1)) == -1)
    {
        perror("shm_key");
        exit(6);
    }
    shm_id = shmget(shm_key, (COUNT + 1) * sizeof(char), IPC_CREAT | permissions);
    if (shm_id == -1)
    {
        perror("shm_id");
        exit(7);
    }
    letters = shmat(shm_id, NULL, 0);
    if (letters == (void *)-1)
    {
        perror("shmat");
        exit(8);
    }
    for (int i = 0; i < COUNT; i++)
        letters[i] = 'a' + i % 26;
    letters[COUNT] = '\0';
    if (init_monitor(sem_id))
    {
        perror("init monitor\n");
        exit(9);
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        perror("socket");
        exit(2);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("bind");
        exit(3);
    }
    if (listen(listen_fd, 5) == -1)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    while (!flag)
    {
        client_length = sizeof(client_address);
        if ((connection_fd = accept(listen_fd, (struct sockaddr *)&client_address, &client_length)) == -1)
        {
            perror("accept");
            exit(1);
        }
        if ((child_pid = fork()) == 0)
        {
            close(listen_fd);
            process_client(sem_id, letters, connection_fd);
            close(connection_fd);
        }
        else
        {
            close(connection_fd);
        }
    }
    if (shmdt(letters) == -1)
    {
        perror("shmdt");
        exit(1);
    }
    close(listen_fd);
    return 0;
}
