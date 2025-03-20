#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "stdio.h"
#include "unistd.h"
#include <time.h>

#define SERVER_FILENAME "task_02.srv"

int main(int argc, char *argv[])
{
	srand(time(NULL));
	int socket_fd;
	int flags = 0;
	char send_buffer[1024] = {};
	char recv_buf[1024] = {};
	struct sockaddr server_address = {.sa_family = AF_UNIX};
	struct sockaddr client_address = {.sa_family = AF_UNIX};
	int rc;

	pid_t client_pid = getpid();
	snprintf(client_address.sa_data, 14, "%d.cl", client_pid);
	snprintf(server_address.sa_data, 14, "%s", SERVER_FILENAME);

	socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket_fd < 0)
	{
		perror("Can't create socket");
		exit(EXIT_FAILURE);
	}

	rc = bind(
		socket_fd,
		&client_address,
		sizeof(client_address.sa_family) + (strlen(client_address.sa_data) + 1) * sizeof(char));

	if (rc < 0)
	{
		perror("Can't bind socket");
		exit(EXIT_FAILURE);
	}

	printf("Client %d started; socket file is %s\n", client_pid, client_address.sa_data);

	alarm(10);
	while (1)
	{
		int num1 = rand() % 10;
		int num2 = rand() % 10;
		snprintf(send_buffer, 1024, "%d + %d", num1, num2);

		rc = sendto(
			socket_fd,
			send_buffer,
			(strlen(send_buffer) + 1) * sizeof(char),
			flags, &server_address,
			sizeof(server_address.sa_family) + (strlen(server_address.sa_data) + 1) * sizeof(char));

		if (rc < 0)
		{
			perror("Can't bind socket");
			exit(EXIT_FAILURE);
		}

		printf("Client %d sent \"%s\" to %.*s\n", client_pid, send_buffer, (int)(strlen(server_address.sa_data) + 1), server_address.sa_data);

		// receive response from server to <client_pid>.cln
		rc = recvfrom(socket_fd, recv_buf, sizeof(recv_buf), flags, NULL, NULL);
		if (rc < 0)
		{
			perror("Can't recvfrom");
			exit(EXIT_FAILURE);
		}
		printf("Client %d received \"%s\"\n", client_pid, recv_buf);
	}

	rc = close(socket_fd);
	rc = unlink(client_address.sa_data);
	return 0;
}
