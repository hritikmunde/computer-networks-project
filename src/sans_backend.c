#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "include/rudp.h"
#include "include/cust_header.h"

#define WINDOW_SIZE 10
void configure_address(struct sockaddr **sock_addr, socklen_t *addr_len, int socket);

typedef struct {
  int sock_fd;
  int pkt_length;
  rudp_packet_t* pkt;
} window_entry_t;

window_entry_t send_window[WINDOW_SIZE];  /*  Buffer containing packets awaiting acknowledgment */

static int total_sent = 0;
static int total_count = 0;
static int buffer_head = 0;
static int buffer_rear = 0;

int get_total_sent() {
  return total_sent;
}

void configure_address(struct sockaddr **sock_addr, socklen_t *addr_len, int socket) {
  for (size_t i = 0; i <  SOCKET_BUFF; i++) {
    if (sock_map[i].sock_id == socket) {
        *sock_addr = sock_map[i].sock_addr;
        *addr_len = sock_map[i].addr_len;
        break;
    }
  }
}
/*
 * Helper: We recommend writing a helper function for pushing a packet onto the ring buffer
 *         that can be called from the main thread.
 *    (Remember to copy the packet to avoid thread conflicts!)
 */
void enqueue_packet(int sock, rudp_packet_t* pkt, int len) {
  rudp_packet_t *new_packet = malloc(sizeof(rudp_packet_t) + len);
  new_packet->type = pkt->type;
  new_packet->seqnum = pkt->seqnum;
  memcpy(new_packet->payload, pkt->payload, len);
  while(total_count - total_sent > WINDOW_SIZE);
  window_entry_t entry;
  entry.sock_fd = sock;
  entry.pkt_length = sizeof(rudp_packet_t) + len;
  entry.pkt = new_packet;
  send_window[buffer_rear] = entry;
  buffer_rear = (buffer_rear + 1) % WINDOW_SIZE;
  total_count = total_count + 1;
}

/*
 * Helper: We recommend writing a helper function for removing a completed packet from the ring
 *         buffer that can be called from the backend thread.
 */
static void dequeue_packet(void) {
  free(send_window[buffer_head].pkt);
  total_sent = total_sent + 1;
  buffer_head = (buffer_head + 1) % WINDOW_SIZE;
}

/*  Asynchronous runner for the sans protocol driver.  This function  *
 *  is responsible for checking for packets in the `send_window` and  *
 *  handling the transmission of these packets in a reliable manner.  *
 *  (i.e. Stop-and-Wait or Go-Back-N)                                 */
void* sans_backend(void* unused) {
  
  while (1) {
    /* ---- Background worker code ---- */
    if(total_count - total_sent > 0) {
        int i = buffer_head;
        while (i != buffer_rear) {
          struct sockaddr *sock_addr;
          socklen_t addr_len;
          configure_address(&sock_addr, &addr_len, send_window[i].sock_fd);
          sendto(send_window[i].sock_fd, send_window[i].pkt, send_window[i].pkt_length, 0, sock_addr, addr_len);
          i = (i+1)%WINDOW_SIZE;
        }
        struct timeval timeout = { .tv_usec = 20000 };
        i = buffer_head;
        int curr_rear = buffer_rear;
        while (i != curr_rear) {
          struct sockaddr *sock_addr;
          socklen_t addr_len;
          configure_address(&sock_addr, &addr_len, send_window[i].sock_fd);
          setsockopt(send_window[i].sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
          rudp_packet_t response;
          int bytes_received = recvfrom(send_window[i].sock_fd, &response, sizeof(response), 0, sock_addr,&addr_len);
          if(bytes_received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
          }
          if(bytes_received > 0 && (response.type & ACK) && response.seqnum == send_window[i].pkt->seqnum) {
            i = (i+1)%WINDOW_SIZE;
            dequeue_packet();
          }
        }
    }
    /*----------------------------------*/
  }

  return NULL;
}
