#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

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

#define THREAD_OK 0
#define THREAD_EXIT_SUCCESS 1
#define THREAD_EXIT_FAILURE 2
#define THREAD_EXIT_ARRAY_FULL 3

int listenfd;
int array_counter = 0;
char array[ARRAY_SIZE];

// Блокировка для чтения-записи
pthread_rwlock_t rwlock;
// Мьютекс для счетчика массива
pthread_mutex_t counter_mutex;

struct thread_args
{
    int socketfd;
};

// Функция чтения
int reader(char *buf, int socketfd)
{
    char buffer[ARRAY_SIZE];

    // Блокировка чтения
    pthread_rwlock_rdlock(&rwlock);
    memcpy(buffer, buf, sizeof(buffer));
    pthread_rwlock_unlock(&rwlock);

    int rc = htonl(OK);
    if (send(socketfd, &rc, sizeof(rc), 0) == -1)
    {
        perror("write()");
        return THREAD_EXIT_FAILURE;
    }
    if (send(socketfd, &buffer, sizeof(buffer), 0) == -1)
    {
        perror("write()");
        return THREAD_EXIT_FAILURE;
    }
    return THREAD_OK;
}

// Функция записи
int writer(char *buf, int socketfd, int index)
{
    int rc = OK;
    char result = '\0';
    int done = THREAD_OK;

    // Блокировка записи
    pthread_rwlock_wrlock(&rwlock);

    if (index < 0 || index >= ARRAY_SIZE)
    {
        rc = OUT_OF_RANGE_ERROR;
    }
    else if (buf[index] == ARRAY_ELEMENT_OCCUPIED)
    {
        rc = WRITE_OCCUPIED_ERROR;
    }
    else
    {
        result = buf[index];
        buf[index] = ARRAY_ELEMENT_OCCUPIED;

        pthread_mutex_lock(&counter_mutex);
        array_counter++;
        if (array_counter == ARRAY_SIZE)
        {
            done = THREAD_EXIT_ARRAY_FULL;
        }
        pthread_mutex_unlock(&counter_mutex);
    }

    if (done != THREAD_EXIT_ARRAY_FULL)
    {
        printf("write request, index %d\n", index);
    }

    pthread_rwlock_unlock(&rwlock);

    rc = htonl(rc);
    if (send(socketfd, &rc, sizeof(rc), 0) == -1)
    {
        perror("write()");
    }
    if (result != '\0')
    {
        if (send(socketfd, &result, sizeof(result), 0) == -1)
        {
            perror("write()");
        }
    }

    return done;
}

// Функция потока для обработки клиентских соединений
void *handle_client(void *arg)
{
    struct thread_args *args = (struct thread_args *)arg;
    int socketfd = args->socketfd;
    free(args); // Освобождаем память, выделенную для args

    int size;
    int type;
    while ((size = recv(socketfd, &type, sizeof(type), 0)) > 0)
    {
        type = ntohl(type);
        switch (type)
        {
        case READ:
        {
            printf("read request\n");
            if (reader(array, socketfd) != THREAD_OK)
            {
                close(socketfd);
                pthread_exit(NULL);
            }
            break;
        }
        case WRITE:
        {
            int index;
            if ((size = recv(socketfd, &index, sizeof(index), 0)) < 0)
            {
                perror("read()");
                close(socketfd);
                pthread_exit(NULL);
            }
            else if (size == 0)
            {
                printf("conn closed\n");
                close(socketfd);
                pthread_exit(NULL);
            }
            index = ntohl(index);
            int done = writer(array, socketfd, index);
            if (done == THREAD_EXIT_ARRAY_FULL)
            {
                close(socketfd);
                kill(getpid(), SIGINT);
                pthread_exit(NULL);
            }
            else if (done != THREAD_OK)
            {
                close(socketfd);
                pthread_exit(NULL);
            }
            break;
        }
        default:
        {
            printf("unexpected request\n");
            int err = htonl(INVALID_OPERATION_ERROR);
            if (send(socketfd, &err, sizeof(err), 0) == -1)
            {
                perror("write()");
                close(socketfd);
                pthread_exit(NULL);
            }
            break;
        }
        }
    }

    close(socketfd);
    if (size == -1)
    {
        perror("read()");
    }
    else if (size == 0)
    {
        printf("Conn closed\n");
    }
    pthread_exit(NULL);
}

// Обработчик сигнала SIGINT
void sigint_handler(int sig)
{
    if (close(listenfd) == -1)
    {
        perror("close()");
        exit(EXIT_FAILURE);
    }
    pthread_rwlock_destroy(&rwlock);
    pthread_mutex_destroy(&counter_mutex);
    printf("Server stopped.\n");
    exit(EXIT_SUCCESS);
}

int main()
{
    // Инициализация массива
    memset(array, ARRAY_ELEMENT_FREE, ARRAY_SIZE);
    for (int i = 0; i < ARRAY_SIZE; i++)
    {
        array[i] = 'a' + i % 26;
    }

    // Инициализация примитивов синхронизации
    if (pthread_rwlock_init(&rwlock, NULL) != 0)
    {
        perror("pthread_rwlock_init()");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_init(&counter_mutex, NULL) != 0)
    {
        perror("pthread_mutex_init()");
        pthread_rwlock_destroy(&rwlock);
        exit(EXIT_FAILURE);
    }

    // Настройка сокета
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(PORT);
    sa.sin_addr.s_addr = INADDR_ANY;
    if (bind(listenfd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        perror("bind()");
        exit(EXIT_FAILURE);
    }
    if (listen(listenfd, 5) == -1)
    {
        perror("listen()");
        exit(EXIT_FAILURE);
    }
    printf("Server started on port %d.\n", PORT);
    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        perror("signal()");
        exit(EXIT_FAILURE);
    }

    // Основной цикл: принимаем соединения и создаем поток для каждого
    for (;;)
    {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd == -1)
        {
            perror("accept()");
            exit(EXIT_FAILURE);
        }

        struct thread_args *args = malloc(sizeof(struct thread_args));
        if (args == NULL)
        {
            perror("malloc()");
            close(connfd);
            continue;
        }
        args->socketfd = connfd;

        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        int err = pthread_create(&thread, &attr, handle_client, args);
        if (err != 0)
        {
            perror("pthread_create()");
            free(args);
            close(connfd);
        }
        pthread_attr_destroy(&attr);
    }
}
