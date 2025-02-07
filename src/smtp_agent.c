#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "include/sans.h" 

#define FILE_END "\r\n.\r\n"
#define SMTP_HELO "HELO"
#define SMTP_MAIL_FROM "MAIL FROM:"
#define SMTP_RCPT_TO "RCPT TO:"
#define SMTP_DATA "DATA"
#define SMTP_QUIT "QUIT"
#define CHUNK_SIZE 512  // This is the chunk size to read emails

// This function reads the email file and sends it in chunks over the SMTP connection.
// It appends the FILE_END string if the file does not contain it.
int send_email_file(int connection, const char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        printf("Error: Unable to open file %s\n", file_path);
        return -1;
    }

    char size_buff[CHUNK_SIZE];
    int file_bytes;

    // Read and send the file contents in chunks
    while ((file_bytes = fread(size_buff, 1, CHUNK_SIZE, file)) > 0) {
        sans_send_pkt(connection, size_buff, file_bytes);
    }

    // Always append the end-of-data string to terminate the email.
    sans_send_pkt(connection, FILE_END, strlen(FILE_END));

    fclose(file);
    return 0;
}

int smtp_agent(const char *host, int port) {
    char sender_receiver[256], file_path[256];

    printf("Enter sender and receiver (same value):\n");
    scanf("%255s", sender_receiver);

    printf("Enter path to the email file:\n");
    scanf("%255s", file_path);

    // Connect to the SMTP server
    int connection = sans_connect(host, port, IPPROTO_TCP);
    if (connection < 0) {
        printf("Error: Failed to connect to %s:%d\n", host, port);
        return -1;
    }

    char response[512];

    // 220 from server
    sans_recv_pkt(connection, response, sizeof(response));
    printf("%s", response); 

    // Send HELO message
    snprintf(response, sizeof(response), "%s %s\r\n", SMTP_HELO, "localhost");
    sans_send_pkt(connection, response, strlen(response));
    sans_recv_pkt(connection, response, sizeof(response));
    printf("%s", response); 

    // Send MAIL FROM message
    snprintf(response, sizeof(response), "%s <%s>\r\n", SMTP_MAIL_FROM, sender_receiver);
    sans_send_pkt(connection, response, strlen(response));
    sans_recv_pkt(connection, response, sizeof(response));
    printf("%s", response); 

    // Send RCPT TO message
    snprintf(response, sizeof(response), "%s <%s>\r\n", SMTP_RCPT_TO, sender_receiver);
    sans_send_pkt(connection, response, strlen(response));
    sans_recv_pkt(connection, response, sizeof(response));
    printf("%s", response); 

    // Send DATA command
    snprintf(response, sizeof(response), "%s\r\n", SMTP_DATA);
    sans_send_pkt(connection, response, strlen(response));
    sans_recv_pkt(connection, response, sizeof(response));
    printf("%s", response);  
    if (strncmp(response, "354", 3) != 0) {
        printf("Error: DATA initiation failed: %s\n", response);
        return -1;
    }

    // Send the email file in chunks
    if (send_email_file(connection, file_path) != 0) {
        printf("Error: Failed to send email file.\n");
        return -1;
    }

    sans_recv_pkt(connection, response, sizeof(response));
    printf("%s", response); 

    // Send QUIT command
    snprintf(response, sizeof(response), "%s\r\n", SMTP_QUIT);
    sans_send_pkt(connection, response, strlen(response));
    sans_recv_pkt(connection, response, sizeof(response));
    printf("%s", response); 
    sans_disconnect(connection);
    return 0;
}

