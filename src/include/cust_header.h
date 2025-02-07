#include <netinet/in.h>
#include <stddef.h>
#define SOCKET_BUFF 10

typedef struct {
    int sock_id;
    struct sockaddr *sock_addr;
    socklen_t addr_len;
    int seq_num;
} sock_entry_t;

extern sock_entry_t sock_map[SOCKET_BUFF];