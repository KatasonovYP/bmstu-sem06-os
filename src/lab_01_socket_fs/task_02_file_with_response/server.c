// <pid>.srv <pid>.clnt

#include "stdlib.h"
#include "string.h"
#include "sys/socket.h"
#include "stdio.h"
#include "unistd.h"

#define SERVER_FILENAME "task_02.srv"

double calculate(double num1, double num2, char operator)
{
	switch (operator)
	{
	case '+':
		return num1 + num2;
	case '-':
		return num1 - num2;
	case '*':
		return num1 * num2;
	case '/':
		if (num2 != 0)
		{
			return num1 / num2;
		}
		else
		{
			printf("Error: Division by zero is not allowed.\n");
			return 0;
		}
	default:
		printf("Error: Unknown operator.\n");
		return 0;
	}
}

// Функция для вычисления выражения, заданного строкой
double evaluate_expression(const char *expression)
{
	double num1, num2;
	char operator;

	if (sscanf(expression, "%lf %c %lf", &num1, &operator, & num2) == 3)
	{
		return calculate(num1, num2, operator);
	}
	else
	{
		printf("Error: Invalid expression format.\n");
		return 0;
	}
}

int main()
{
	pid_t server_pid = getpid();
	struct sockaddr server_address = {.sa_family = AF_UNIX};
	struct sockaddr client_address = {.sa_family = AF_UNIX};
	socklen_t client_address_len = sizeof(client_address);
	snprintf(server_address.sa_data, 14, "%s", SERVER_FILENAME);
	char send_buffer[1024] = {};
	int flags = 0;

	int socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (socket_fd < 0)
	{
		perror("Can't create socket");
		exit(EXIT_FAILURE);
	}

	int rc = bind(socket_fd, &server_address, sizeof(server_address.sa_family) + (strlen(server_address.sa_data) + 1) * sizeof(char));
	if (rc < 0)
	{
		perror("Can't bind socket");
		exit(EXIT_FAILURE);
	}

	printf("Server with pid %d started; socket file is %s\n", server_pid, server_address.sa_data);

	alarm(30);

	while (1)
	{
		char received_buffer[1024] = {};

		rc = recvfrom(
			socket_fd,
			received_buffer,
			sizeof(received_buffer),
			flags,
			(struct sockaddr *)&client_address,
			&client_address_len);

		if (rc < 0)
		{
			perror("Can't recvfrom");
			exit(EXIT_FAILURE);
		}

		printf("Server received \"%s\" from %s\n", received_buffer, client_address.sa_data);

		sprintf(send_buffer, "%.0f", evaluate_expression(received_buffer));

		rc = sendto(
			socket_fd,
			send_buffer,
			(strlen(send_buffer) + 1) * sizeof(char), flags, &client_address,
			sizeof(client_address.sa_family) + (strlen(client_address.sa_data) + 1) * sizeof(char));

		if (rc < 0)
		{
			perror("Can't sendto");
			exit(EXIT_FAILURE);
		}
		printf("Server sent \"%s\" to %s\n", send_buffer, client_address.sa_data);
	}

	rc = close(socket_fd);
	if (rc < 0)
	{
		perror("Can't close");
		exit(EXIT_FAILURE);
	}

	rc = unlink(server_address.sa_data);
	if (rc < 0)
	{
		perror("Can't unlink");
		exit(EXIT_FAILURE);
	}
	return 0;
}