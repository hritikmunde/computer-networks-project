#include "include/sans.h"
#include <stdio.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <string.h>

#define MAX_PACKET_SIZE 1024

long get_file_size(const char *path) {                                                          // Function to extract Content-Length
    struct stat file_stat;
    if (stat(path, &file_stat) == 0)
    {
        return file_stat.st_size;
    }
    return -1;
}

int http_server(const char* iface, int port) {
    int client_socket = sans_accept(iface, port, IPPROTO_TCP);                                  // Accept a connection from a remote client or disconnect
    if (client_socket < 0) {
        printf("Error: Client connection could not be established!\n");
        return 1;
    }

    char request_buff[MAX_PACKET_SIZE];
    int packet_length = sans_recv_pkt(client_socket, request_buff, sizeof(request_buff));       // Receive the request from the client
    if (packet_length < 0) {
        printf("Error: Could not receive client request\n");
        sans_disconnect(client_socket);
        return 1;
    }

    char method[10], file_path[256];
    sscanf(request_buff, "%s /%s", method, file_path);                                          // assuming a GET request

    long file_size = get_file_size(file_path);
    if (file_size < 0) {
        char file_err[] = "HTTP/1.1 404 Not Found\r\n"                                          // Sending a 404 if file not found
                        "Content-Length: 9\r\n"
                        "Content-Type: text/html; charset=utf-8\r\n"
                        "\r\n"
                        "Not Found";
        sans_send_pkt(client_socket, file_err, strlen(file_err));
        sans_disconnect(client_socket);
        return 1;
    }

    FILE *file = fopen(file_path, "r");                                                         // Open the requested file/content
    if (!file) {
        char server_error[] = "HTTP/1.1 500 Internal Server Error\r\n"                          // Keeping this for a 500 error if there is any
                            "Content-Length: 21\r\n"
                            "Content-Type: text/html; charset=utf-8\r\n"
                            "\r\nInternal Server Error";
        sans_send_pkt(client_socket, server_error, strlen(server_error));
        sans_disconnect(client_socket);
        return 1;
    }

    char header_rec[MAX_PACKET_SIZE];
    int header_len = snprintf(header_rec, sizeof(header_rec),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %ld\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "\r\n", file_size);
    
    sans_send_pkt(client_socket, header_rec, header_len);

    char file_buff[MAX_PACKET_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buff, 1, sizeof(file_buff), file)) > 0) 
    {
        sans_send_pkt(client_socket, file_buff, bytes_read);
    }

    fclose(file);
    sans_disconnect(client_socket);
    return 0;
}