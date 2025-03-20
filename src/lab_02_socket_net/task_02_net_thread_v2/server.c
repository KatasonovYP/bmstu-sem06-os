#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>

#define BUFFER_SIZE 28
#define ACTIVE_READERS 0
#define ACTIVE_WRITER 1
#define WAITING_WRITERS 2
#define SERVER_PORT 5470
#define MAX_CLIENTS 10
#define SA struct sockaddr

int semid;
char buffer[BUFFER_SIZE];
pthread_t p_thread;
pthread_attr_t attr;
struct reqv_arg
{
    int connfd;
    char *buffer;
    int buffer_len;
    int semid;
    int thread_num;
};

struct sembuf start_read[] = {
    {ACTIVE_WRITER, 0, 0},
    {WAITING_WRITERS, 0, 0},
    {ACTIVE_READERS, 1, 0},
};

struct sembuf stop_read[] = {
    {ACTIVE_READERS, -1, 0}};

struct sembuf start_write[] = {
    {WAITING_WRITERS, 1, 0},
    {ACTIVE_READERS, 0, 0},
    {ACTIVE_WRITER, 0, 0},
    {ACTIVE_WRITER, 1, 0},
    {WAITING_WRITERS, -1, 0}};

struct sembuf stop_write[] =
    {
        {ACTIVE_WRITER, -1, 0}};

void error_log(const char *message)
{
    printf("%s", message);
    perror(message);
    exit(EXIT_FAILURE);
}

void reader(int semid, int connfd, struct sockaddr *clientaddr, char *buffer)
{
    if (semop(semid, start_read, 3) == -1)
        error_log("Semop");
    if (sendto(connfd, buffer, (strlen(buffer) + 1), 0, clientaddr, sizeof(*clientaddr)) == -1)
        error_log("Sendto");
    if (semop(semid, stop_read, 1) == -1)
        error_log("Semop");
};

int writer(int semid, int connfd, struct sockaddr *clientaddr, char *buffer)
{
    int msg[1];
    int c_bytes;
    int addrlen = sizeof(clientaddr);
    if ((c_bytes = recvfrom(connfd, msg, sizeof(msg), 0, (SA *)&clientaddr, &addrlen)) == -1)
        error_log("Recvfrom");
    if (semop(semid, start_write, 5) == -1)
        error_log("Semop");
    if (buffer[msg[0]] == '_')
        msg[0] = -1;
    else
    {
        buffer[msg[0]] = '_';
    }
    if (sendto(connfd, msg, sizeof(msg), 0, clientaddr, sizeof(*clientaddr)) == -1)
        error_log("Sendto");
    if (semop(semid, stop_write, 1) == -1)
        error_log("Semop");
}

void *myfunWithMultiThreads(void *arg)
{
    cpu_set_t mask;
    cpu_set_t get;
    int i = 0;
    int num = 0;
    int cpuID = *(int *)arg;
    CPU_ZERO(&mask);
    CPU_SET(cpuID, &mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) == -1)
    {
        fprintf(stderr, "Set thread  affinity failed\n");
        fprintf(stdout, "Set thread  affinity failed\n");
    }
    CPU_ZERO(&get);
    if (pthread_getaffinity_np(pthread_self(), sizeof(get), &get) == -1)
    {
        fprintf(stderr, "Get thread  affinity failed\n");
        fprintf(stdout, "Get thread  affinity failed\n");
    }
    printf("hhh%d\n", sched_getcpu());
    num = sysconf(_SC_NPROCESSORS_CONF);
    for (i = 0; i < CPU_SETSIZE; i++)
    {
        if (CPU_ISSET(i, &get))
        {
            printf("Thread %d running in processor %ld\n", pthread_self(), i);
            printf("Original setting is processor %d\n\n", cpuID);
        }
    }

    /*int s;
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();


    CPU_ZERO(&cpuset);
    for (size_t j = 0; j < 8; j++)
        CPU_SET(j, &cpuset);

    s = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
    if (s != 0)
        error_log( "pthread_setaffinity_np");


    s = pthread_getaffinity_np(thread, sizeof(cpuset), &cpuset);
    if (s != 0)
        error_log("pthread_getaffinity_np");

    printf("Set returned by pthread_getaffinity_np() contained:\n");
    for (size_t j = 0; j < CPU_SETSIZE; j++)
        if (CPU_ISSET(j, &cpuset))
            printf("    CPU %zu\n", j);*/
}

//(const int connfd, char *buffer, const int buffer_len, int semid)
void *proc_reqv(void *argv_ptr)
{
    struct sockaddr_in clientaddr;
    struct reqv_arg *args = argv_ptr;
    int connfd = args->connfd;
    char *buffer = args->buffer;
    int buffer_len = args->buffer_len;
    int semid = args->semid;
    int thread_num = args->thread_num;
    int addrlen = sizeof(clientaddr);
    int c_bytes;
    int clientpid[1];
    thread_num = 2;
    if ((c_bytes = recvfrom(connfd, clientpid, sizeof(clientpid), 0, (SA *)&clientaddr, &addrlen)) == -1)
        error_log("Recvfrom");
    printf("Pid: %d \n", clientpid[0]);
    while (1)
    {
        // myfunWithMultiThreads((void *)&thread_num);
        if (buffer[buffer_len - 3] == '_')
            pthread_exit(EXIT_SUCCESS);
        reader(semid, connfd, (SA *)&clientaddr, buffer);
        writer(semid, connfd, (SA *)&clientaddr, buffer);
    }
    pthread_exit(EXIT_SUCCESS);
};

void init_buffer(char *buffer, int buffer_len)
{
    buffer[0] = 'A';
    for (int i = 1; i < buffer_len - 1; i++)
        buffer[i] = buffer[i - 1] + 1;
    buffer[buffer_len] = '\0';
}

int main(void)
{
    int connfd, childpid, c_bytes;
    int c_threads = 0;
    const int perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
        error_log("Socket");
    struct sockaddr_in serveraddr, client_addr;
    int clilen = sizeof(client_addr);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVER_PORT);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) == -1)
        error_log("Bind");
    if (listen(listenfd, MAX_CLIENTS) == -1)
        error_log("Listen");

    key_t semkey = ftok("key.txt", 1);
    if (semkey == -1)
        error_log("Ftok");

    if ((semid = semget(semkey, 5, IPC_CREAT | perms)) == -1)
        error_log("Semget");
    if (semctl(semid, ACTIVE_READERS, SETVAL, 0) == -1)
        error_log("Semctl");
    if (semctl(semid, WAITING_WRITERS, SETVAL, 0) == -1)
        error_log("Semctl");
    if (semctl(semid, ACTIVE_WRITER, SETVAL, 0) == -1)
        error_log("Semctl");
    init_buffer(buffer, BUFFER_SIZE);
    while (buffer[BUFFER_SIZE - 3] != '_')
    {
        if ((connfd = accept(listenfd, (SA *)&client_addr, &clilen)) == -1)
            error_log("Accept");
        struct reqv_arg *argv_ptr = (struct reqv_arg *)malloc(sizeof(struct reqv_arg));
        argv_ptr->buffer = buffer;
        argv_ptr->semid = semid;
        argv_ptr->connfd = connfd;
        argv_ptr->buffer_len = BUFFER_SIZE;
        argv_ptr->thread_num = c_threads;
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&p_thread, &attr, proc_reqv, (void *)argv_ptr);
        c_threads++;
    }

    close(listenfd);
    // sleep(5);
    if (semctl(semid, 1, IPC_RMID, NULL) == -1)
        error_log("Semctl");
    return 0;
}
