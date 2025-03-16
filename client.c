#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")

SOCKET client_socket;
char username[512];

DWORD WINAPI receiveMessages(LPVOID lpParam) {
	char buffer[1024];

	while (1) {
		memset(buffer, 0, sizeof(buffer));
		int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

		if (bytes_read <= 0) {
			printf("\nDisconnected.\n");
			closesocket(client_socket);
			WSACleanup();
			exit(0);
		}

		buffer[bytes_read] = '\0';
		printf("\r\033[K"); // '\r' moves to the start, '\033[K' clears the line
		printf("%s\n", buffer);
		printf("Enter a message (or type 'exit' to quit): ");
		fflush(stdout);
	}

	return 0;
}

int main(void) {

	fprintf(stdout, "Enter your username: ");
	fgets(username, 512, stdin);

	username[strcspn(username, "\n")] = '\0';
	snprintf(username + strlen(username), sizeof(username) - strlen(username), "%s", ": ");

	// Initialize Winsock
	WSADATA winsock_data;
	if (WSAStartup(MAKEWORD(2, 2), &winsock_data) != 0) {
		fprintf(stderr, "WSAStartup failed with error: %d\n", WSAGetLastError());
		return 1;
	}

	// Create socket
	client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == INVALID_SOCKET) {
		fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// Server address setup
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(9001);
	// Convert IP address from text to binary
	if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) != 1) {
		fprintf(stderr, "Invalid address / Adress not supported\n");
		closesocket(client_socket);
		WSACleanup();
		return 1;
	}

	// Connect to the server
	if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		fprintf(stderr, "Connction failed: %d\n", WSAGetLastError());
		closesocket(client_socket);
		WSACleanup();
		return 1;
	}

	printf("STATUS: Connected to the server.\n");

	// Start the receiving thread
	HANDLE receive_thread = CreateThread(NULL, 0, receiveMessages, NULL, 0, NULL);
	if (receive_thread == NULL) {
		fprintf(stderr, "Failed to create thread.\n");
		closesocket(client_socket);
		WSACleanup();
		return 1;
	}

	char message[1024];

	while (1) {
		char full_message[1024];
		strcpy_s(full_message, sizeof(full_message), username);

		printf("Enter a message (or type 'exit' to quit): ");
		fgets(message, 1024, stdin);
		message[strcspn(message, "\n")] = '\0';

		if (strcmp(message, "exit") == 0) break;

		// Use snprintf to safely concatenate strings
		snprintf(full_message + strlen(full_message), sizeof(full_message) - strlen(full_message), "%s", message);

		int iResult = send(client_socket, full_message, (int)strlen(full_message), 0);
		if (iResult == SOCKET_ERROR) {
			fprintf(stderr, "Send failed with error: %d\n", WSAGetLastError());
			closesocket(client_socket);
			WSACleanup();
			return 1;
		}
	}
	
	// Cleanup
	CloseHandle(receive_thread);
	closesocket(client_socket);
	WSACleanup();
	return 0;
}