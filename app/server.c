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

#include "http.h"

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
	/* parse request */ {
		char *method = strtok(buffer, " ");
		char *path = strtok(method + strlen(method) + 1, " ");
		char *version = strtok(path + strlen(path) + 1, " ");

		printf("method=`%s` path=`%s` version=`%s`\n", method, path, version);

		request.method = method_valueof(method);
		request.path = strdup(path);

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

		if (request.method == POST)
		{
			int content_length = atoi(headers_get(request.headers, CONTENT_LENGTH) ?: "0");
			unsigned char *body = malloc(content_length);

			if (recv(client_fd, body, content_length, 0) == -1)
			{
				perror("recv ~ request body");
				return (1);
			}

			request.body = body;
			request.body_length = content_length;
		}
	}

	response_t response = {};
	response.status = NOT_FOUND;

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

		if (request.method == GET)
		{
			int fd = open(path, O_RDONLY);
			if (fd != -1)
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
		else if (request.method == POST)
		{
			int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
			if (fd != -1)
			{
				write(fd, request.body, request.body_length);

				close(fd);

				response.status = CREATED;
			}
		}
	}

	if (response.body)
	{
		const char *accept_encodings = headers_get(request.headers, ACCEPT_ENCODING);
		if (accept_encodings)
		{
			encoder_t encoder = NULL;

			char *accept_encoding = strtok((char*)accept_encodings, COMA);
			while (accept_encoding)
			{
				while (*accept_encoding == ' ')
					++accept_encoding;

				encoder = encoder_get(accept_encoding);
				if (encoder)
				{
					response.headers = headers_add(response.headers, CONTENT_ENCODING, accept_encoding);
					break;
				}

				accept_encoding = strtok(NULL, COMA);
			}

			if (encoder)
			{
				unsigned char *old_body = response.body;
				response.body_length = encoder(old_body, response.body_length, &response.body);
				free(old_body);
			}
		}

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
	free(request.body);

	headers_clear(response.headers);
	free(response.body);

	close(server_fd);

	return 0;
}
