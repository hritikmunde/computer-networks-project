#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include "include/sans.h"
#include "include/rudp.h"
#include "include/cust_header.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_CONNECTIONS 10
#define PACKET_SIZE 1400
#define MAX_ATTEMPTS 10

sock_entry_t sock_map[SOCKET_BUFF];

void add_socket(int sfd, struct sockaddr *sock_addr, socklen_t addr_len) {
    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        if (sock_map[i].sock_id == 0) {
            sock_map[i].sock_id = sfd;
            sock_map[i].addr_len = addr_len;
            sock_map[i].sock_addr = malloc(addr_len);
            memcpy(sock_map[i].sock_addr, sock_addr, addr_len);
            sock_map[i].seq_num = 2;
            break;
        }
    }
}

int sans_connect(const char* host, int port, int protocol) {
    if (protocol != IPPROTO_TCP && protocol != IPPROTO_RUDP) {
        return -1;
    }
    char port_string[10];
    snprintf(port_string, sizeof(port_string), "%d", port);
    struct addrinfo hints, *result, *rp;
    int sfd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = (protocol == IPPROTO_RUDP) ? SOCK_DGRAM : SOCK_STREAM;

    int s = getaddrinfo(host, port_string, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;
        if (protocol == IPPROTO_TCP) {
            if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == -1) {
                close(sfd);
                sfd = -1;
                continue;
            }
            break;
        } else {
            struct timeval timeout = { .tv_usec = 20000 };
            setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            
            rudp_packet_t request = { .type = SYN, .seqnum = 0 };
            int attempts = 0;
            while (attempts < MAX_ATTEMPTS) {
                socklen_t addr_len = rp->ai_addrlen;
                sendto(sfd, &request, sizeof(request), 0, rp->ai_addr, addr_len);
                rudp_packet_t response;
                int bytes_received = recvfrom(sfd, &response, sizeof(response), 0, rp->ai_addr, &addr_len);
                if (bytes_received > 0 && (response.type & SYN) && (response.type & ACK)) {
                    rudp_packet_t ack_packet = { .type = ACK, .seqnum = 1 };
                    sendto(sfd, &ack_packet, sizeof(ack_packet), 0, rp->ai_addr, addr_len);
                    add_socket(sfd, rp->ai_addr, addr_len);
                    break;
                }
                attempts++;
            }
            if (attempts == MAX_ATTEMPTS) {
                close(sfd);
                sfd = -1;
            }
        }
    }

    freeaddrinfo(result);
    return sfd;
}

int sans_accept(const char* iface, int port, int protocol) {
  if (protocol != IPPROTO_TCP && protocol != IPPROTO_RUDP) {
    return -1;
  }
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd = -1;
  char port_string[10];
  snprintf(port_string, sizeof(port_string), "%d", port);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;  
  if(protocol == IPPROTO_RUDP)
  {
    hints.ai_socktype = SOCK_DGRAM;
  }
  else
  {
    hints.ai_socktype = SOCK_STREAM;
  }

  int s = getaddrinfo(iface, port_string, &hints, &result);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(EXIT_FAILURE);
  }
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1) {
      continue;
    }
    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == -1) {
        perror("Bind failed");
        close(sfd);
        sfd = -1;
        continue;
    }
    if(protocol == IPPROTO_TCP) {

      if (listen(sfd, MAX_CONNECTIONS) == -1) {
        close(sfd);
        sfd = -1;
        continue;
      }
      add_socket(sfd, rp->ai_addr, rp->ai_addrlen);
      int new_socket = accept(sfd, NULL, NULL);
      close(sfd);
      return new_socket;
    }
    else{
      socklen_t addr_len = rp->ai_addrlen;
      while(1) {
        rudp_packet_t request;
        int bytes_received = recvfrom(sfd, &request, sizeof(request), 0, rp->ai_addr, &addr_len);
        if (bytes_received > 0 && request.type & SYN){
          int x = 0;
          struct timeval timeout = {
            .tv_usec = 20000
          };
          setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
          rudp_packet_t syn_ack_packet, ack_packet;
          syn_ack_packet.type = SYN | ACK;
          syn_ack_packet.seqnum = request.seqnum;
          while(x < MAX_ATTEMPTS) {
            sendto(sfd, &syn_ack_packet, sizeof(rudp_packet_t), 0, rp->ai_addr,(socklen_t) rp->ai_addrlen);
            socklen_t addr_len = rp->ai_addrlen;
            int bytes_received = recvfrom(sfd, &ack_packet, sizeof(ack_packet), 0, rp->ai_addr, &addr_len);
            if (bytes_received >= 0 && (ack_packet.type & ACK) ) {
              add_socket(sfd, rp->ai_addr, rp->ai_addrlen);
              return sfd;
            }
            else {
              perror("Error receiving ACK");
              x++;
            }
          }
        }
      }
    }
  }

  freeaddrinfo(result);
  
  return -1;
}

int sans_disconnect(int socket) {
  if(socket != -1) {
    return close(socket);
  }
  return -1;
}