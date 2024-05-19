#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

typedef enum
{
	GET,
	POST,
	UNKNOWN
} method_t;

typedef enum
{
	OK,
	NOT_FOUND
} status_t;

typedef struct
{
	method_t method;
	const char *path;
} request_t;

typedef struct
{
	status_t status;
} response_t;

static const int STATUS_TO_CODE[] = {
	[OK] = 200,
	[NOT_FOUND] = 404,
};

static const char *STATUS_TO_PHRASE[] = {
	[OK] = "OK",
	[NOT_FOUND] = "Not Found",
};

method_t method_valueof(const char *input)
{
	if (strcmp("GET", input) == 0)
		return (GET);
	
	if (strcmp("POST", input) == 0)
		return (POST);
	
	return (UNKNOWN);
}

size_t recv_line(int fd, char *buffer, size_t length)
{
	size_t index;

	for (index = 0; index < length - 1; ++index)
	{

		char value = 0;

		if (recv(fd, &value, 1, 0) != 1)
		{
			return (-1);
		}

		if (value == '\n')
		{
			break;
		}

		buffer[index] = value;
	}

	if (index != 0 && buffer[index - 1] == '\r')
	{
		index--;
	}

	buffer[index] = '\0';
	return (index);
}

int main()
{
	setbuf(stdout, NULL);
	printf("codecrafters build-your-own-http\n");

	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		perror("socket");
		return 1;
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
	{
		perror("setsockopt(SO_REUSEPORT)");
		return 1;
	}

	int port = 4221;
	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		perror("bind");
		return 1;
	}

	printf("listen: %d\n", port);

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		perror("listen");
		return 1;
	}

	int client_addr_len = sizeof(struct sockaddr_in);
	struct sockaddr_in client_addr;
	int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
	if (client_fd == -1)
	{
		perror("accept");
		return 1;
	}

	printf("Client connected\n");

	char buffer[512];
	if (recv_line(client_fd, buffer, sizeof(buffer)) == -1)
	{
		perror("recv_line");
		return 1;
	}

	request_t request;

	{
		char *method = strtok(buffer, " ");
		char *path = strtok(method + strlen(method) + 1, " ");
		char *version = strtok(path + strlen(path) + 1, " ");

		printf("method=`%s` path=`%s` version=`%s`\n", method, path, version);

		request.method = method_valueof(method);
		request.path = strdup(path);
	}

	// while (true)
	// {
	// 	size_t length = recv_line(client_fd, buffer, sizeof(buffer));
	// 	if (length == -1)
	// 	{
	// 		perror("recv_line");
	// 		return 1;
	// 	}

	// 	if (length == 0)
	// 	{
	// 		break;
	// 	}

	// 	printf("`%s`\n", buffer);
	// 	fflush(stdout);
	// }

	response_t response = {};

	if (strcmp("/", request.path) == 0)
	{
		response.status = OK;
	}
	else
	{
		response.status = NOT_FOUND;
	}

	int status_code = STATUS_TO_CODE[response.status];
	const char *status_phrase = STATUS_TO_PHRASE[response.status];
	sprintf(buffer, "HTTP/1.1 %d %s\r\n\r\n", status_code, status_phrase);

	if (send(client_fd, buffer, strlen(buffer), 0) == -1)
	{
		perror("send");
		return 1;
	}

	close(server_fd);

	return 0;
}
