#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <winerror.h>

#pragma comment(lib, "Ws2_32.lib")

#define MAX_CLIENTS 10
#define PORT 9001
#define BUFFER_SIZE 1024

typedef struct {
	OVERLAPPED overlapped;
	SOCKET socket;
	char buffer[BUFFER_SIZE];
	WSABUF wsa_buffer;
} IO_OPERATION;

typedef struct {
	SOCKET socket;
	char username[512];
} Client;

HANDLE iocp;
SOCKET client_sockets[MAX_CLIENTS] = { 0 };

void broadcast_message(const char* message, SOCKET sender);
void handle_client_data(IO_OPERATION* operation, DWORD bytes_transferred);
DWORD WINAPI worker_thread(LPVOID lpParam);

int main(void) {
	WSADATA winsock_data;
	if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
		fprintf(stderr, "WSAStartup failed.\n");
		return 1;
	}

	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == INVALID_SOCKET) {
		fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		fprintf(stderr, "Bind failed: %d\n", WSAGetLastError());
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}

	if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
		fprintf(stderr, "Listen failed: %d\n", WSAGetLastError());
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}

	printf("Server listening on port %d...\n", PORT);

	// Create IOCP - I/O Completion Port
	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (iocp == NULL) {
		fprintf(stderr, "CreateIoCompletionPort failed: %d\n", GetLastError());
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}

	// Create worker threads
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	for (int i = 0; i < 2; i++) {	// OPTIONAL: Change 2 to sysinfo.dwNumberOfProcessors
		CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
	}

	while (1) {
		struct sockaddr_in client_addr;
		int addrlen = sizeof(client_addr);
		SOCKET new_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addrlen);
		if (new_socket == INVALID_SOCKET) {
			fprintf(stderr, "Accept failed: %d\n", WSAGetLastError());
			continue;
		}

		printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

		// Store the client socket
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (client_sockets[i] == 0) {
				client_sockets[i] = new_socket;
				break;
			}
		}

		// Associate socket with IOCP
		IO_OPERATION* operation = (IO_OPERATION*)malloc(sizeof(IO_OPERATION));
		if (operation == NULL) {
			printf("Failed to allocate memory for IO_OPERATION.\n");
			closesocket(new_socket);
			continue;
		}

		memset(operation, 0, sizeof(IO_OPERATION));
		operation->socket = new_socket;
		operation->wsa_buffer.buf = operation->buffer;
		operation->wsa_buffer.len = BUFFER_SIZE;

		if (CreateIoCompletionPort((HANDLE)new_socket, iocp, (ULONG_PTR)operation, 0) == NULL) {
			printf("CreateIoCompletionPort failed: %d", GetLastError());
			free(operation);
			closesocket(new_socket);
			continue;
		}

		// Start receiving data asynchronously
		DWORD bytes_received;
		DWORD flags = 0;
		int recv_result = WSARecv(new_socket, &operation->wsa_buffer, 1, &bytes_received, &flags, &operation->overlapped, NULL);
		if (recv_result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
			printf("Error starting async receive: %d\n", WSAGetLastError());
			free(operation);
			closesocket(new_socket);
		}
	}

	closesocket(server_socket);
	WSACleanup();
	return 0;
}

void broadcast_message(const char* message, SOCKET sender) {
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_sockets[i] != sender && client_sockets[i] != 0) {
			send(client_sockets[i], message, (int)strlen(message), 0);
		}
	}
}

void handle_client_data(IO_OPERATION* operation, DWORD bytes_transferred) {
	if (bytes_transferred == 0) {
		printf("Client disconnected.\n");
		closesocket(operation->socket);
		free(operation);
		return;
	}

	operation->buffer[bytes_transferred] = '\0';
	printf("Received: %s\n", operation->buffer);

	// Broadcast messages to all clients
	broadcast_message(operation->buffer, operation->socket);

	// Reissue WSARecv for next message
	DWORD bytes_received;
	DWORD flags;
	int recv_result = WSARecv(operation->socket, &operation->wsa_buffer, 1, &bytes_received, &flags, &operation->overlapped, NULL);
	if (recv_result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		printf("Error starting async receive: %d\n", WSAGetLastError());
		closesocket(operation->socket);
		free(operation);
	}
}

DWORD WINAPI worker_thread(LPVOID lpParam) {
	while (1) {
		DWORD bytes_transferred;
		ULONG_PTR completion_key;
		LPOVERLAPPED lpOverlapped;

		// Wait for an I/O Completion packet
		BOOL result = GetQueuedCompletionStatus(iocp, &bytes_transferred, &completion_key, &lpOverlapped, INFINITE);
		if (!result) {
			fprintf(stderr, "GetQueuedCompletionStatus failed: %d\n", GetLastError());
			continue;
		}

		IO_OPERATION* operation = (IO_OPERATION*)lpOverlapped;
		handle_client_data(operation, bytes_transferred);
	}

	return 0;
}