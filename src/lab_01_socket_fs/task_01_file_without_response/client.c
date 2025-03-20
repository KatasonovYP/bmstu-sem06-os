#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "stdio.h"
#include "unistd.h"

#define SERVER_FILENAME "task_01.srv"

int main()
{
	int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket_fd < 0)
	{
		perror("Can't create socket");
		exit(EXIT_FAILURE);
	}

	struct sockaddr socket_address = {.sa_family = AF_UNIX};
	snprintf(socket_address.sa_data, 14, "%s", SERVER_FILENAME);

	pid_t client_pid = getpid();
	printf("Client with pid %d started\n", client_pid);

	int flags = 0;

	char send_buffer[1024] = {};
	snprintf(send_buffer, 1024, "%d", client_pid);

	sendto(socket_fd, send_buffer, (strlen(send_buffer) + 1) * sizeof(char), flags, &socket_address,
		   sizeof(socket_address.sa_family) + (strlen(socket_address.sa_data) + 1) * sizeof(char));
	printf("Client %d sent \"%s\" to %.*s\n", client_pid, send_buffer, (int)(strlen(socket_address.sa_data) + 1), socket_address.sa_data);
	return 0;
}
