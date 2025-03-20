#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

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

#define MAX_EVENTS 10
#define MAX_THREADS 4

// Состояния для конечного автомата
#define STATE_READ_COMMAND 0
#define STATE_READ_INDEX 1
#define STATE_WRITE_RESPONSE 2
#define STATE_WRITE_DATA 3

// Структура для хранения информации о клиентской сессии
typedef struct
{
    int fd;
    int state;
    int command;
    int index;
    int response_code;
    char result;
    char buffer[ARRAY_SIZE];
    int bytes_processed;
    int bytes_to_process;
} client_session_t;

int epollfd;
int listenfd;
int array_counter = 0;
char array[ARRAY_SIZE];

// Синхронизация
pthread_rwlock_t rwlock;
pthread_mutex_t counter_mutex;

// Пул потоков
pthread_t worker_threads[MAX_THREADS];
int should_exit = 0;

// Функция для установки сокета в неблокирующий режим
void set_nonblocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL O_NONBLOCK");
        exit(EXIT_FAILURE);
    }
}

// Обработчик сигнала SIGINT
void sigint_handler(int sig)
{
    should_exit = 1;
    // Закрываем epoll дескриптор, что приведет к выходу из epoll_wait
    close(epollfd);
    close(listenfd);
    printf("Server stopping...\n");
}

// Освобождение ресурсов клиентской сессии
void free_client_session(client_session_t *session)
{
    if (session)
    {
        if (session->fd != -1)
        {
            close(session->fd);
        }
        free(session);
    }
}

// Добавление нового клиентского соединения в epoll
void add_client(int client_fd)
{
    // Создаем новую сессию
    client_session_t *session = calloc(1, sizeof(client_session_t));
    if (!session)
    {
        perror("calloc");
        close(client_fd);
        return;
    }

    session->fd = client_fd;
    session->state = STATE_READ_COMMAND;
    session->bytes_processed = 0;
    session->bytes_to_process = sizeof(int);

    // Устанавливаем сокет в неблокирующий режим
    set_nonblocking(client_fd);

    // Добавляем клиентский сокет в epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; // Используем Edge-Triggered режим
    ev.data.ptr = session;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
    {
        perror("epoll_ctl: add client");
        free_client_session(session);
        return;
    }

    printf("New client connected: %d\n", client_fd);
}

// Чтение данных от клиента
int handle_read(client_session_t *session)
{
    int bytes_left = session->bytes_to_process - session->bytes_processed;
    char *buffer_ptr = (char *)&session->command + session->bytes_processed;
    struct epoll_event ev; // Declare only once at the beginning of the function

    if (session->state == STATE_READ_INDEX)
    {
        buffer_ptr = (char *)&session->index + session->bytes_processed;
    }

    int n = read(session->fd, buffer_ptr, bytes_left);
    if (n == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // Нет данных для чтения
            return 0;
        }
        perror("read");
        return -1;
    }
    else if (n == 0)
    {
        // Соединение закрыто
        printf("Client disconnected: %d\n", session->fd);
        return -1;
    }

    session->bytes_processed += n;

    // Если прочитали все необходимые данные
    if (session->bytes_processed == session->bytes_to_process)
    {
        if (session->state == STATE_READ_COMMAND)
        {
            session->command = ntohl(session->command);

            switch (session->command)
            {
            case READ:
                printf("Read request from client %d\n", session->fd);
                // Блокировка чтения
                pthread_rwlock_rdlock(&rwlock);
                memcpy(session->buffer, array, ARRAY_SIZE);
                pthread_rwlock_unlock(&rwlock);

                session->response_code = OK;
                session->state = STATE_WRITE_RESPONSE;
                session->bytes_processed = 0;
                session->bytes_to_process = sizeof(int);

                ev.events = EPOLLOUT | EPOLLET;
                ev.data.ptr = session;
                if (epoll_ctl(epollfd, EPOLL_CTL_MOD, session->fd, &ev) == -1)
                {
                    perror("epoll_ctl: mod to write");
                    return -1;
                }
                break;

            case WRITE:
                printf("Write request from client %d\n", session->fd);
                // Переходим к чтению индекса
                session->state = STATE_READ_INDEX;
                session->bytes_processed = 0;
                session->bytes_to_process = sizeof(int);
                break;

            default:
                printf("Invalid operation from client %d\n", session->fd);
                session->response_code = INVALID_OPERATION_ERROR;
                session->state = STATE_WRITE_RESPONSE;
                session->bytes_processed = 0;
                session->bytes_to_process = sizeof(int);

                ev.events = EPOLLOUT | EPOLLET;
                ev.data.ptr = session;
                if (epoll_ctl(epollfd, EPOLL_CTL_MOD, session->fd, &ev) == -1)
                {
                    perror("epoll_ctl: mod to write");
                    return -1;
                }
                break;
            }
        }
        else if (session->state == STATE_READ_INDEX)
        {
            session->index = ntohl(session->index);
            printf("Write index %d from client %d\n", session->index, session->fd);

            // Блокировка записи
            pthread_rwlock_wrlock(&rwlock);

            if (session->index < 0 || session->index >= ARRAY_SIZE)
            {
                session->response_code = OUT_OF_RANGE_ERROR;
            }
            else if (array[session->index] == ARRAY_ELEMENT_OCCUPIED)
            {
                session->response_code = WRITE_OCCUPIED_ERROR;
            }
            else
            {
                session->result = array[session->index];
                array[session->index] = ARRAY_ELEMENT_OCCUPIED;

                pthread_mutex_lock(&counter_mutex);
                array_counter++;
                int is_full = (array_counter == ARRAY_SIZE);
                pthread_mutex_unlock(&counter_mutex);

                if (is_full)
                {
                    kill(getpid(), SIGINT);
                }

                session->response_code = OK;
            }

            pthread_rwlock_unlock(&rwlock);

            // Переходим к отправке ответа
            session->state = STATE_WRITE_RESPONSE;
            session->bytes_processed = 0;
            session->bytes_to_process = sizeof(int);

            ev.events = EPOLLOUT | EPOLLET;
            ev.data.ptr = session;
            if (epoll_ctl(epollfd, EPOLL_CTL_MOD, session->fd, &ev) == -1)
            {
                perror("epoll_ctl: mod to write");
                return -1;
            }
        }
    }

    return 0;
}

// Отправка данных клиенту
int handle_write(client_session_t *session)
{
    int bytes_left = session->bytes_to_process - session->bytes_processed;
    char *buffer_ptr;

    if (session->state == STATE_WRITE_RESPONSE)
    {
        int response = htonl(session->response_code);
        buffer_ptr = (char *)&response + session->bytes_processed;
    }
    else if (session->state == STATE_WRITE_DATA)
    {
        if (session->command == READ)
        {
            buffer_ptr = session->buffer + session->bytes_processed;
        }
        else
        { // WRITE
            buffer_ptr = &session->result + session->bytes_processed;
        }
    }

    int n = write(session->fd, buffer_ptr, bytes_left);
    if (n == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // Буфер отправки полон
            return 0;
        }
        perror("write");
        return -1;
    }

    session->bytes_processed += n;

    // Если отправили весь ответ
    if (session->bytes_processed == session->bytes_to_process)
    {
        if (session->state == STATE_WRITE_RESPONSE)
        {
            if (session->response_code == OK)
            {
                if (session->command == READ)
                {
                    // Если это был запрос на чтение, отправляем данные
                    session->state = STATE_WRITE_DATA;
                    session->bytes_processed = 0;
                    session->bytes_to_process = ARRAY_SIZE;
                    return 0;
                }
                else if (session->command == WRITE && session->result != '\0')
                {
                    // Если это был успешный запрос на запись, отправляем символ
                    session->state = STATE_WRITE_DATA;
                    session->bytes_processed = 0;
                    session->bytes_to_process = sizeof(char);
                    return 0;
                }
            }
        }

        // Возвращаемся к чтению команды
        session->state = STATE_READ_COMMAND;
        session->bytes_processed = 0;
        session->bytes_to_process = sizeof(int);

        // Меняем события на чтение
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = session;
        if (epoll_ctl(epollfd, EPOLL_CTL_MOD, session->fd, &ev) == -1)
        {
            perror("epoll_ctl: mod to read");
            return -1;
        }
    }

    return 0;
}

// Функция потока-обработчика событий
void *worker_thread(void *arg)
{
    struct epoll_event events[MAX_EVENTS];

    while (!should_exit)
    {
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, 1000); // Ожидаем с таймаутом в 1 секунду
        if (nfds == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.ptr == NULL)
            {
                // Это событие на слушающем сокете
                int client_fd;
                while ((client_fd = accept(listenfd, NULL, NULL)) != -1)
                {
                    add_client(client_fd);
                }
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    perror("accept");
                }
            }
            else
            {
                client_session_t *session = (client_session_t *)events[i].data.ptr;

                if (events[i].events & EPOLLIN)
                {
                    if (handle_read(session) < 0)
                    {
                        free_client_session(session);
                        continue;
                    }
                }

                if (events[i].events & EPOLLOUT)
                {
                    if (handle_write(session) < 0)
                    {
                        free_client_session(session);
                        continue;
                    }
                }

                if (events[i].events & (EPOLLERR | EPOLLHUP))
                {
                    printf("Error on client socket %d\n", session->fd);
                    free_client_session(session);
                    continue;
                }
            }
        }
    }

    pthread_exit(NULL);
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
        perror("pthread_rwlock_init");
        exit(EXIT_FAILURE);
    }

    if (pthread_mutex_init(&counter_mutex, NULL) != 0)
    {
        perror("pthread_mutex_init");
        pthread_rwlock_destroy(&rwlock);
        exit(EXIT_FAILURE);
    }

    // Создание epoll дескриптора
    epollfd = epoll_create1(0);
    if (epollfd == -1)
    {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Создание слушающего сокета
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Установка опции переиспользования адреса
    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    // Установка неблокирующего режима
    set_nonblocking(listenfd);

    // Привязка к адресу
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    // Начинаем прослушивание
    if (listen(listenfd, SOMAXCONN) == -1)
    {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    // Добавляем слушающий сокет в epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = NULL; // NULL для слушающего сокета, чтобы отличать от клиентских

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
    {
        perror("epoll_ctl: listen_sock");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    // Устанавливаем обработчик сигнала
    signal(SIGINT, sigint_handler);

    printf("Server started on port %d.\n", PORT);

    // Создаем рабочие потоки
    for (int i = 0; i < MAX_THREADS; i++)
    {
        if (pthread_create(&worker_threads[i], NULL, worker_thread, NULL) != 0)
        {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // Ожидаем завершения рабочих потоков
    for (int i = 0; i < MAX_THREADS; i++)
    {
        pthread_join(worker_threads[i], NULL);
    }

    // Освобождаем ресурсы
    close(listenfd);
    close(epollfd);
    pthread_rwlock_destroy(&rwlock);
    pthread_mutex_destroy(&counter_mutex);

    printf("Server stopped.\n");
    return 0;
}
