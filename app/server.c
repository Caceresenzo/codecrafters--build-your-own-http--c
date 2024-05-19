#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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
	if (client_fd  == -1) {
		perror("accept");
		return 1;
	}

	printf("Client connected\n");

	char response_line[] = "HTTP/1.1 200 OK\r\n\r\n";
	if (send(client_fd, response_line, strlen(response_line), 0) == -1) {
		perror("send");
		return 1;
	}

	close(server_fd);

	return 0;
}
