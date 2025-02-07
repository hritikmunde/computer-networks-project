#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/sans.h"

#define MAX_PACKET_SIZE 1024

char *cust_strstr(const char *haystack, const char *needle) {
    const char *h;
    const char *n;
    
    if (!*needle) return (char *)haystack;

    for (; *haystack; haystack++) {
        h = haystack;
        n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
    }

    return NULL;
}

void parse_request(char *request, const char *method, const char *path, const char *host, int port) {
    snprintf(request, MAX_PACKET_SIZE, 
             "%s /%s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "User-Agent: sans/1.0\r\n"
             "Cache-Control: no-cache\r\n"
             "Connection: close\r\n"
             "Accept: */*\r\n\r\n", 
             method, (*path == '/') ? path + 1 : path, host, port);
}

int send_my_request(int socket_id, const char *request) {
    if (sans_send_pkt(socket_id, request, strlen(request)) < 0) {
        sans_disconnect(socket_id);
        return -1;
    }
    return 0;
}

int cust_header(char *response, int *content_length) {
    char *header_end = cust_strstr(response, "\r\n\r\n");
    if (header_end) {
        char *content_length_ptr = cust_strstr(response, "Content-Length:");
        if (content_length_ptr) {
            content_length_ptr += strlen("Content-Length: ");
            *content_length = strtol(content_length_ptr, NULL, 10);
        }
        return header_end + 4 - response;
    }
    return 0;
}

void get_response(int socket_id, char *response, int *content_length) {
    int bytes_received;
    int headers_done = 0;

    while ((bytes_received = sans_recv_pkt(socket_id, response, MAX_PACKET_SIZE - 1)) > 0) {
        response[bytes_received] = '\0';
        printf("%s", response);

        if (!headers_done) {
            int header_size = cust_header(response, content_length);
            if (header_size) {
                headers_done = 1;
                bytes_received -= header_size;
                *content_length -= bytes_received;
            }
        } else {
            if (*content_length > 0) {
                *content_length -= bytes_received;
                if (*content_length <= 0) break;
            }
        }
    }
}

int http_client(const char *host, int port) {
    char method[5];
    char path[256];
    char request[MAX_PACKET_SIZE];
    char response[MAX_PACKET_SIZE];
    int socket_id, content_length = -1;

    scanf("%s %s", method, path);
    if (strcmp(method, "GET") != 0) 
    {
        return 0;
    }

    socket_id = sans_connect(host, port, IPPROTO_TCP);
    if (socket_id < 0) 
    {
        return 0;
    }

    parse_request(request, method, path, host, port);
    if (send_my_request(socket_id, request) < 0) 
    {
        return 0;
    }

    get_response(socket_id, response, &content_length);

    sans_disconnect(socket_id);
    return 0;
}

