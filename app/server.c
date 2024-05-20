#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>

#define CONTENT_TYPE "Content-Type"
#define CONTENT_LENGTH "Content-Length"
#define USER_AGENT "User-Agent"
#define TEXT_PLAIN "text/plain"
#define APPLICATION_OCTET_STREAM "application/octet-stream"

typedef struct header_s
{
	char *key;
	char *value;
	struct header_s *next;
} header_t;

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
	char *path;
	header_t *headers;
} request_t;

typedef struct
{
	status_t status;
	header_t *headers;
	unsigned char *body;
	size_t body_length;
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

header_t *headers_add(header_t *previous, const char *key, const char *value)
{
	header_t *header = malloc(sizeof(header_t));

	header->key = strdup(key);
	header->value = strdup(value);
	header->next = previous;

	return (header);
}

header_t *headers_add_number(header_t *previous, const char *key, size_t value)
{
	char buffer[32];
	sprintf(buffer, "%ld", value);

	return (headers_add(previous, key, buffer));
}

void headers_clear(header_t *header)
{
	while (header)
	{
		free(header->key);
		free(header->value);

		header_t *next = header->next;
		free(header);
		header = next;
	}
}

const char *headers_get(header_t *first, const char *key)
{
	header_t *header = first;
	while (header)
	{
		if (strcasecmp(key, header->key) == 0)
			return (header->value);

		header = header->next;
	}

	return ("");
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

int main(int argc, char **argv)
{
	setbuf(stdout, NULL);
	printf("codecrafters build-your-own-http\n");

	if (argc == 3)
	{
		const char *directory = argv[2];

		printf("directory: %s\n", directory);
		chdir(directory);
	}

	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		perror("socket");
		return (1);
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
	{
		perror("setsockopt(SO_REUSEPORT)");
		return (1);
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
		return (1);
	}

	printf("listen: %d\n", port);

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		perror("listen");
		return (1);
	}

	int client_fd;
	while (true)
	{
		int client_addr_len = sizeof(struct sockaddr_in);
		struct sockaddr_in client_addr;
		client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		if (client_fd == -1)
		{
			perror("accept");
			return (1);
		}

		printf("Client connected\n");

		pid_t pid = fork();
		if (pid == -1)
		{
			perror("fork");
			return (1);
		}

		if (pid != 0)
		{
			close(client_fd);
			continue;
		}

		break;
	}

	char buffer[512];
	if (recv_line(client_fd, buffer, sizeof(buffer)) == -1)
	{
		perror("recv_line");
		return (1);
	}

	request_t request = {};
	{
		char *method = strtok(buffer, " ");
		char *path = strtok(method + strlen(method) + 1, " ");
		char *version = strtok(path + strlen(path) + 1, " ");

		printf("method=`%s` path=`%s` version=`%s`\n", method, path, version);

		request.method = method_valueof(method);
		request.path = strdup(path);
	}

	while (true)
	{
		size_t length = recv_line(client_fd, buffer, sizeof(buffer));
		if (length == -1)
		{
			perror("recv_line");
			return (1);
		}

		if (length == 0)
		{
			break;
		}

		char *key = strtok(buffer, ": ");
		char *value = key + strlen(key) + 1;

		while (*value == ' ')
			++value;

		request.headers = headers_add(request.headers, key, value);
	}

	response_t response = {};

	if (strcmp("/", request.path) == 0)
	{
		response.status = OK;
	}
	else if (strncmp("/echo/", request.path, 6) == 0)
	{
		char *body = strdup(request.path + 6);

		response.status = OK;
		response.headers = headers_add(response.headers, CONTENT_TYPE, TEXT_PLAIN);
		response.body = body;
		response.body_length = strlen(body);
	}
	else if (strcmp("/user-agent", request.path) == 0)
	{
		char *body = strdup(headers_get(request.headers, USER_AGENT));

		response.status = OK;
		response.headers = headers_add(response.headers, CONTENT_TYPE, TEXT_PLAIN);
		response.body = body;
		response.body_length = strlen(body);
	}
	else if (strncmp("/files/", request.path, 7) == 0)
	{
		char *path = request.path + 7;

		int fd = open(path, O_RDONLY);
		if (fd == -1)
		{
			response.status = NOT_FOUND;
		}
		else
		{
			struct stat statbuf;
			fstat(fd, &statbuf);

			size_t length = statbuf.st_size;
			unsigned char *body = malloc(length);
			read(fd, body, length);

			close(fd);

			response.status = OK;
			response.headers = headers_add(response.headers, CONTENT_TYPE, APPLICATION_OCTET_STREAM);
			response.body = body;
			response.body_length = length;
		}
	}
	else
	{
		response.status = NOT_FOUND;
	}

	if (response.body)
	{
		response.headers = headers_add_number(response.headers, CONTENT_LENGTH, response.body_length);
	}

	{
		int status_code = STATUS_TO_CODE[response.status];
		const char *status_phrase = STATUS_TO_PHRASE[response.status];

		sprintf(buffer, "HTTP/1.1 %d %s\r\n", status_code, status_phrase);
	}

	if (send(client_fd, buffer, strlen(buffer), 0) == -1)
	{
		perror("send ~ request line");
		return (1);
	}

	header_t *header = response.headers;
	while (header)
	{
		sprintf(buffer, "%s: %s\r\n", header->key, header->value);

		if (send(client_fd, buffer, strlen(buffer), 0) == -1)
		{
			perror("send ~ header");
			return (1);
		}

		header = header->next;
	}

	if (send(client_fd, "\r\n", 2, 0) == -1)
	{
		perror("send ~ request header end");
		return (1);
	}

	if (response.body)
	{
		if (send(client_fd, response.body, response.body_length, 0) == -1)
		{
			perror("send ~ body");
			return (1);
		}
	}

	free(request.path);
	headers_clear(request.headers);

	headers_clear(response.headers);
	free(response.body);

	close(server_fd);

	return 0;
}
