/* udp-server.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//#define MAXBUFFER 8192
#define MAXBUFFER 8
#define ADDRBUFFER 128

int main()
{
  int sd;  /* socket descriptor -- this is actually in the fd table! */

  /* create the socket (endpoint) on the server side */
  sd = socket( AF_INET, SOCK_DGRAM, 0 );
                    /*  ^^^^^^^^^^
                       this will set this socket up to use UDP */

  if ( sd == -1 )
  {
    perror( "socket() failed" );
    return EXIT_FAILURE;
  }

  printf( "Server-side socket created on descriptor %d\n", sd );


  struct sockaddr_in server;
  int length;

  server.sin_family = AF_INET;  /* IPv4 */

  server.sin_addr.s_addr = htonl( INADDR_ANY );
           /* any remote IP can send us a datagram */

  /* specify the port number for the server */
  server.sin_port = htons( 0 );  /* a 0 here means let the kernel assign
                                    us a port number to listen on */

  /* bind to a specific (OS-assigned) port number */
  if ( bind( sd, (struct sockaddr *) &server, sizeof( server ) ) < 0 )
  {
    perror( "bind() failed" );
    return EXIT_FAILURE;
  }

  length = sizeof( server );

  /* call getsockname() to obtain the port number that was just assigned */
  if ( getsockname( sd, (struct sockaddr *) &server, (socklen_t *) &length ) < 0 )
  {
    perror( "getsockname() failed" );
    return EXIT_FAILURE;
  }

  printf( "UDP server at port number %d\n", ntohs( server.sin_port ) );





  /* the code below implements the application protocol */
  int n;
  char buffer[ MAXBUFFER ];
  char addr_buffer[ ADDRBUFFER ];
  struct sockaddr_in client;
  int len = sizeof( client );
  int backup_len = len;

  while ( 1 )
  {
    /* read a datagram from the remote client side (BLOCKING) */
    n = recvfrom( sd, buffer, MAXBUFFER, 0, (struct sockaddr *) &client,
                  (socklen_t *) &len );

    if ( n == -1 )
    {
      perror( "recvfrom() failed" );
    }
    else
    {
      //inet_ntoa is deprecated, only handles IPV4
      printf( "Rcvd datagram from %s port %d\n",
              inet_ntoa( client.sin_addr ), ntohs( client.sin_port ) );
      printf( "Rcvd datagram from %s port %d\n",
              inet_ntop( AF_INET, &client.sin_addr, addr_buffer, ADDRBUFFER ), ntohs( client.sin_port ) );
      //TODO: Add a check in case inet_ntop returns NULL.

      printf( "RCVD %d bytes\n", n );
      buffer[n] = '\0';   /* assume that its printable char[] data */
      printf( "RCVD: [%s]\n", buffer );

      /* echo the data back to the sender/client */
      //IMPORTANT: len is the length set by recvfrom. Might not match backup_len, could lead to bugs!
      sendto( sd, buffer, n, 0, (struct sockaddr *) &client, len );
      //sendto( sd, buffer, n, 0, (struct sockaddr *) &client, backup_len );

      /* to do: check the return code of sendto() */
    }
  }

  return EXIT_SUCCESS;
}
