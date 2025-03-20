#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "stdio.h"
#include "unistd.h"

#define TIMEOUT 10
#define SERVER_FILENAME "task_01.srv"

int main()
{
	int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket_fd < 0)
	{
		perror("Can't create socket");
		exit(EXIT_FAILURE);
	}

	pid_t server_pid = getpid();
	struct sockaddr socket_address = {.sa_family = AF_UNIX};
	snprintf(socket_address.sa_data, 14, "%s", SERVER_FILENAME);

	socklen_t socket_length = sizeof(socket_address.sa_family) + (strlen(socket_address.sa_data) + 1) * sizeof(char);

	int bind_rc = bind(socket_fd, &socket_address, socket_length);
	if (bind_rc < 0)
	{
		perror("Can't bind socket");
		exit(EXIT_FAILURE);
	}

	printf("Server started; socket file is %s\n", socket_address.sa_data);

	int alarm_rc = alarm(TIMEOUT);
	if (alarm_rc < 0)
	{
		perror("Can't alarm");
		exit(EXIT_FAILURE);
	}

	int flags = 0;
	char buffer[1024] = {};

	int recvfrom_rc = recvfrom(socket_fd, buffer, sizeof(buffer), flags, NULL, NULL);
	if (recvfrom_rc < 0)
	{
		perror("Can't recvfrom");
		exit(EXIT_FAILURE);
	}

	printf("Server received \"%s\"\n", buffer);

	int close_rc = close(socket_fd);
	if (close_rc < 0)
	{
		perror("Can't close");
		exit(EXIT_FAILURE);
	}

	int unlink_rc = unlink(socket_address.sa_data);
	if (unlink_rc < 0)
	{
		perror("Can't unlink");
		exit(EXIT_FAILURE);
	}
	return 0;
}