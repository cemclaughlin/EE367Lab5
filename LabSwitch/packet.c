
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "main.h"
#include "packet.h"
#include "net.h"
#include "host.h"


void packet_send(struct net_port *port, struct packet *p)
{
    char msg[PAYLOAD_MAX+4];
    int i;

    if (port->type == PIPE) {
        msg[0] = (char) p->src;
        msg[1] = (char) p->dst;
        msg[2] = (char) p->type;
        msg[3] = (char) p->length;
        for (i=0; i<p->length; i++) {
            msg[i+4] = p->payload[i];
        }
        write(port->pipe_send_fd, msg, p->length+4);
        //printf("PACKET SEND, src=%d dst=%d p-src=%d p-dst=%d\n",
        //		(int) msg[0],
        //		(int) msg[1],
        //		(int) p->src,
        //		(int) p->dst);
    }
    else if(port->type == SOCKET) {
      struct addrinfo hints, *servinfo, *s;
      int sockfd, rv;

      memset(&hints, 0, sizeof hints);
	    hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;

	    if ((rv = getaddrinfo(port->socket_send_url, port->socket_send_port, &hints, &servinfo)) != 0) {
		    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    		return ;
	    }

	// loop through all the results and connect to the first we can
	    for(s = servinfo; s != NULL; s = s->ai_next) {
    		if ((sockfd = socket(s->ai_family, s->ai_socktype,
				    s->ai_protocol)) == -1) {
			    perror("client: socket");
			    continue;
		    }

		    if (connect(sockfd, s->ai_addr, s->ai_addrlen) == -1) {
			    close(sockfd);
			    perror("client: connect");
			    continue;
		    }

    		break;
	    }

      if (s == NULL) {
		      fprintf(stderr, "client: failed to connect\n");
		        return;
	    }
      //socket created, compile msg
      msg[0] = (char) p->src;
      msg[1] = (char) p->dst;
      msg[2] = (char) p->type;
      msg[3] = (char) p->length;
      for (i=0; i<p->length; i++) {
          msg[i+4] = p->payload[i];
      }
      //need to append a '\0' to msg?

      //may have issues with msg being a char, and sizeof calculations
      if (send(sockfd, msg, p->length + 4, 0) ==-1) perror("send");

      close(sockfd);
    }



    return;
}

int packet_recv(struct net_port *port, struct packet *p)
{
    char msg[PAYLOAD_MAX+4];
    int n;
    int i;

    if (port->type == PIPE) {
        n = read(port->pipe_recv_fd, msg, PAYLOAD_MAX+4);
        if (n>0) {
            p->src = (char) msg[0];
            p->dst = (char) msg[1];
            p->type = (char) msg[2];
            p->length = (int) msg[3];
            for (i=0; i<p->length; i++) {
                p->payload[i] = msg[i+4];
            }

            // printf("PACKET RECV, src=%d dst=%d p-src=%d p-dst=%d\n",
            //		(int) msg[0],
            //		(int) msg[1],
            //		(int) p->src,
            //		(int) p->dst);
        }
    }
    //TODO: SOCKETS
    else if(port->type == SOCKET) {
      printf("attempting to recv \n");
      if ((n = recv(port->pipe_recv_fd, msg, p->length + 4, MSG_DONTWAIT)) == -1) {
        perror("recv");
        exit(1);
      }
      if (n>0) {
          p->src = (char) msg[0];
          p->dst = (char) msg[1];
          p->type = (char) msg[2];
          p->length = (int) msg[3];
          for (i=0; i<p->length; i++) {
              p->payload[i] = msg[i+4];
          }
      }
    }
    return(n);
}
