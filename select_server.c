#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")

#define MAX_CLIENTS 10
#define PORT 9001
#define BUFFER_SIZE 1024

typedef struct {
	SOCKET socket;
	char username[512];
} Client;

int main(void) {

	// Initialize Winsock
	WSADATA winsock_data;
	if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
		fprintf(stderr, "WSAStartup failed.\n");
		return 1;
	}

	// Create socket
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == INVALID_SOCKET) {
		fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// Define the server
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(9001);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	// Bind the socket to the specified IP and port
	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		fprintf(stderr, "Bind failed: %d\n", WSAGetLastError());
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}

	// Listen for incoming connections
	if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
		fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}

	printf("Server listening on port: 9001...\n");

	SOCKET client_sockets[MAX_CLIENTS] = { 0 };
	char client_usernames[MAX_CLIENTS][512] = { 0 };
	struct sockaddr_in client_addr;
	fd_set readfds;

	while (1) {
		int max_fd = server_socket;
		FD_ZERO(&readfds);
		FD_SET(server_socket, &readfds);

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (client_sockets[i] > 0) {
				FD_SET(client_sockets[i], &readfds);
				if (client_sockets[i] > max_fd)
				{
					max_fd = client_sockets[i];
				}
			}
		}

		// Select the socket ready for reading
		int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
		if (activity == SOCKET_ERROR) {
			fprintf(stderr, "Select error: %d\n", WSAGetLastError());
			continue;
		}

		// Check for new client connection
		if (FD_ISSET(server_socket, &readfds)) {
			int addrlen = sizeof(client_addr);
			SOCKET new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addrlen);

			printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

			// Send welcome message
			char welcome_message[] = "Welcome to the chat server!";
			send(new_socket, welcome_message, strlen(welcome_message), 0);

			// Store client socket
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (client_sockets[i] == 0) {
					client_sockets[i] = new_socket;
					strcpy(client_usernames[i], "Johny");
					break;
				}
			}
		}

		for (int i = 0; i < MAX_CLIENTS; i++) {
			SOCKET s = client_sockets[i];
			char user_name = *client_usernames[i];

			if (s > 0 && FD_ISSET(s, &readfds)) {
				char buffer[1024] = { 0 };
				int bytes_read = recv(s, buffer, sizeof(buffer), 0);
				if (bytes_read <= 0) {
					printf("Client disconnected.\n");
					closesocket(s);
					client_sockets[i] = 0;
				}
				else {
					const char general_prefix[] = "GC->";
					char message_with_prefix[2048] = { 0 };

					strcpy(message_with_prefix, general_prefix);
					strcat(message_with_prefix, client_usernames[i]);
					strcat(message_with_prefix, ": ");
					strcat(message_with_prefix, buffer);
					
					//printf("Client %d: %s\n", i + 1, buffer);

					// Send the message to all active clients
					for (int j = 0; j < MAX_CLIENTS; j++) {						
						SOCKET temp_sock = client_sockets[j];
						if (temp_sock > 0) {
							printf("Sent to client: %d\n", temp_sock);
							send(temp_sock, message_with_prefix, (int)strlen(message_with_prefix), 0);
						}
					}
				}
			}
		}

	}

	// Close sockets and cleanup
	closesocket(server_socket);
	WSACleanup();

	return 0;
}