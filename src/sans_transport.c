#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "include/sans.h"
#include "include/rudp.h"
#include "include/cust_header.h"

#define BUFF_LEN 1024

void enqueue_packet(int, rudp_packet_t*, int);
int get_sent();

int sans_send_pkt(int socket, const char* buf, int len) {
  int seq_num;
  for (size_t i = 0; i <  SOCKET_BUFF; i++) {
    if (sock_map[i].sock_id == socket) {
        seq_num = sock_map[i].seq_num;
        sock_map[i].seq_num++;
        break;
    }
  }
  rudp_packet_t *packet = malloc(sizeof(rudp_packet_t) + len);
  packet->type = DAT;
  packet->seqnum = seq_num;
  memcpy(packet->payload, buf, len);
  //int current_count = get_sent();
  //int target = current_count + 1;
  enqueue_packet(socket, packet, len);
  /*while(current_count != target) {
    current_count = get_sent();
  }*/
  return len;
}

int sans_recv_pkt(int socket, char* buf, int len) {
  struct sockaddr *sock_addr;
  socklen_t addr_len;
  int seq_num;
  for (size_t i = 0; i <  SOCKET_BUFF; i++) {
    if (sock_map[i].sock_id == socket) {
        sock_addr = sock_map[i].sock_addr;
        addr_len = sock_map[i].addr_len;
        seq_num = sock_map[i].seq_num;
        sock_map[i].seq_num++;
        break;
    }
  }
  rudp_packet_t *response = malloc(sizeof(rudp_packet_t) + BUFF_LEN);
  int bytes_received = recvfrom(socket, response, sizeof(rudp_packet_t) + BUFF_LEN, 0, sock_addr, &addr_len);
  while(bytes_received < 0 || response->type & DAT || response->seqnum != seq_num) {
    bytes_received = recvfrom(socket, response, sizeof(rudp_packet_t) + BUFF_LEN, 0, sock_addr, &addr_len);
  }
  rudp_packet_t *ack_packet = malloc(sizeof(rudp_packet_t));
  ack_packet->type = ACK;
  ack_packet->seqnum = response->seqnum;
  sendto(socket, ack_packet, sizeof(rudp_packet_t), 0, sock_addr, addr_len);
  int payload_len = bytes_received - sizeof(rudp_packet_t);
  memcpy(buf, response->payload, payload_len);
  buf[payload_len] = '\0';
  return payload_len;
}
