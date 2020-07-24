#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/shm.h> 
#include <sys/ipc.h> 
#include <pthread.h>
#include <ctype.h>

/* Defines */
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 50 

/* Function Prototypes */
int max(int x, int y);
void* tcp_receive(void *arg);
int get_sock_from_userid(char *userid);
int is_new_userid(char *new_userid);
int is_alphanumeric(char* word);
void sort_words(char* words[], int count);


/* Structs */
struct client_struct /* So we can pass void* for threads */
{
  char userid[20]; 
  int socket;
  int sock_tcp;
  int index;
  int active; /* TODO: check if a client is still active */ 
};

/* Globals */ 
struct client_struct clients[MAX_CLIENTS]; // Store all the clients 
// struct client_struct* clients = calloc(MAX_CLIENTS, sizeof(struct client_struct));
int client_index = 0;
char delimeters[] = " \t\r\n\v\f";
pthread_mutex_t lock;

int main(int argc, char const *argv[])
{
  setvbuf(stdout, NULL, _IONBF, 0); // No buffering
  if (argc != 2)
  {
    fprintf(stderr, "ERROR: Invalid argument(s)\n");
    fprintf(stderr, "USAGE: a.out <port number>\n");
    return (EXIT_FAILURE);
  }

  if (pthread_mutex_init(&lock, NULL) != 0)
  {
    fprintf(stderr, "mutex init has failed\n");
    return (EXIT_FAILURE);
  }

  bzero(clients, MAX_CLIENTS);
  int port = atoi(argv[1]);
  fd_set readfds;
  pthread_t tid;
  printf("Main: Started server\n");
  
  /* socket structures from /usr/include/sys/socket.h */
  struct sockaddr_in server;
  struct sockaddr_in client;

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  
  server.sin_port = htons(port);
  int len = sizeof(server);

  /* Start TCP server */
  int sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_tcp < 0)
  {
    perror("socket()");
    exit(EXIT_FAILURE);
  }


  if (bind(sock_tcp, (struct sockaddr *) &server, len) < 0)
  {
    perror("bind()");
    exit(EXIT_FAILURE);
  }

  /* Start UDP server */
  int sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_udp < 0)
  {
    perror("socket()");
    exit(EXIT_FAILURE);
  }

  if(bind(sock_udp, (struct sockaddr*) &server, len))
  {
    perror("bind()");
    exit(EXIT_FAILURE);
  }

  /* Start listening to incoming connections */
  listen(sock_tcp, MAX_CLIENTS);
  printf("MAIN: Listening for TCP connections on port: %d\n", port);
  printf("MAIN: Listening for UDP datagrams on port: %d\n", port);


  int fromlen = sizeof(client);
  char buffer[BUFFER_SIZE];

  FD_ZERO(&readfds);
  int maxfd = max(sock_tcp, sock_udp) + 1; /* Get maxfd... Idk why we do this... */ 
  FD_SET(sock_tcp, &readfds); /* listener socket, fd 3 */
  FD_SET(sock_udp, &readfds); /* listener socket, fd 3 */

  while (1)
  {
    FD_ZERO(&readfds);
    FD_SET(sock_tcp, &readfds); /* listener socket, fd 3 */
    FD_SET(sock_udp, &readfds); /* listener socket, fd 3 */

    // printf("Set FD_SET to include listener fd %d\n", sock_tcp);


    // for (int i = 0; i < client_socket_index; i++)
    // {
    //   FD_SET(client_sockets[i], &readfds);
    //   printf("Set FD_SET to include client socket fd %d\n", client_sockets[i]);
    // }
    
    select(maxfd, &readfds, NULL, NULL, NULL);

    /* ready is the number of ready file descriptors */
    // if (ready != 0)
    // {
    //   printf("select() identified %d descriptor(s) with activity\n", ready);
    // }


    if (FD_ISSET(sock_tcp, &readfds)) /* If there's a TCP connection */
    { 
      pthread_mutex_lock(&lock);
      int newsock = accept(sock_tcp, (struct sockaddr*)&client, (socklen_t*)&fromlen);
      printf("MAIN: Rcvd incoming TCP connection from: %s\n", inet_ntoa((struct in_addr) client.sin_addr));

      struct client_struct  new_client;
      strcpy(new_client.userid, "hjksdfskds"); // Just so we don't get errors just in case 
      new_client.socket = newsock; // Set the socket of the new client
      new_client.sock_tcp = sock_tcp;
      new_client.index = client_index;
      if(client_index > MAX_CLIENTS)
      {
        printf("Too many clients\n");
        pthread_mutex_unlock(&lock);
        continue;
      }

      clients[client_index++] = new_client;
      pthread_mutex_unlock(&lock);
      pthread_create(&tid, NULL, tcp_receive, (void*) &clients[client_index - 1]); // This might not work
      /* We need to check if there is a "WHO" command */
    }

    if (FD_ISSET(sock_udp, &readfds)) /* If there's a UDP connection */
    {
      printf("MAIN: Rcvd incoming UDP datagram from: %s\n", inet_ntoa((struct in_addr)client.sin_addr));
      pthread_mutex_lock(&lock);
      bzero(buffer, BUFFER_SIZE); // Zero out buffer
      int n = recvfrom(sock_udp, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client, (socklen_t *)&fromlen);
      if (n == -1)
      {
        perror("recvfrom() failed");
      }
      buffer[n] = '\n'; // Not all inputs will be null terminated
#if DEBUG_MODE
      // printf("Message is: %s\n", buffer);
#endif
      char *token = strtok(buffer, delimeters);
      char command[50];
      bzero(command, 50);
      strcpy(command, token);
#if DEBUG_MODE
      // printf("Command is %s\n", command);
#endif
      if (strcmp(command, "LOGIN") == 0)
      {
        printf("MAIN: LOGIN not supported over UDP\n");
        n = sendto(sock_udp, "LOGIN not supported over UDP\n", strlen("LOGIN not supported over UDP\n"), 0, (struct sockaddr *)&client, sizeof(client));
        pthread_mutex_unlock(&lock);
        continue;
      }
      else if (strncmp(command, "WHO", 3) == 0)
      {
        printf("MAIN: Rcvd WHO request\n");
        char msg[1024];
        bzero(msg, 1024);
        strcat(msg, "OK!"); // If needed the newline is added in a few lines down
        char **names_sorted = calloc(50, sizeof(char*)); 
        for(int i = 0; i < 50; i++)
        {
          names_sorted[i] = calloc(50, sizeof(char));
        }
        int num_names = 0;
        for (int i = 0; i < client_index; i++)
        {
          if (clients[i].active == 1)
          {
            strcpy(names_sorted[num_names], clients[i].userid);
            num_names++; 
          }
        }

        sort_words(names_sorted, num_names);

        for (int i = 0; i < num_names; i++)
        {
          strcat(msg, "\n");
          strcat(msg, names_sorted[i]);
        }
        strcat(msg, "\n");


#ifdef DEBUG_MODE
        printf("Sending message: %s\n", msg);
#endif
        n = sendto(sock_udp, msg, strlen(msg), 0, (struct sockaddr *)&client, sizeof(client)); // Have to send to client
        for(int i = 0; i < 50; i++)
        {
          free(names_sorted[i]);
        }
        free(names_sorted);
#ifdef DEBUG_MODE
        printf("Sent message!\n");
#endif
      }
      else if (strncmp(command, "BROADCAST", 10) == 0)
      {
        printf("MAIN: Rcvd BROADCAST request\n");
        sendto(sock_udp, "OK!\n", 4, 0, (struct sockaddr *)&client, sizeof(client));
        token = strtok(NULL, delimeters);
        int msglen = atoi(token);
        char msglen_str[10];
        bzero(msglen_str, 10);
        sprintf(msglen_str, "%d", msglen);
        char message[1024];
        bzero(message, 1024);
        token = strtok(NULL, ""); // Get the rest of the message 
        strcat(message, "FROM ");
        strcat(message, "UDP-client ");
        strcat(message, msglen_str);
        strcat(message, " ");
        
        while (strlen(token) < msglen)
        {
          char rest_of_message[BUFFER_SIZE];
          bzero(rest_of_message, BUFFER_SIZE);
          n = recvfrom(sock_udp, rest_of_message, BUFFER_SIZE, 0, (struct sockaddr *)&client, (socklen_t *)&fromlen);
          strcat(token, rest_of_message);
        }

        char dest[1024];     // Destination string
        strncpy(dest, token, msglen); // only use msg characters to string
        dest[msglen] = '\0'; // null terminate destination
        strcat(message, dest);
        strcat(message, "\n");

        // Uh... How do I access all the clients in the thread... Do I have to pass them through the struct?
        if (client_index != 0) // This will always be true
        {
          for (int i = 0; i < client_index; i++)
          {
            if (clients[i].active == 1)
            {
#ifdef DEBUG_MODE
              // printf("Message sending to: %s\n", clients[i].userid);
#endif        
              send(clients[i].socket, message, strlen(message), 0);
#ifdef DEBUG_MODE
              // printf("Message sent to: %s\n", clients[i].userid);
#endif
            }
          }
        }
        pthread_mutex_unlock(&lock);
        continue;
      }
      else if (strncmp(command, "SEND", 5) == 0)
      {
        printf("SEND not supported over UDP\n");
        sendto(sock_udp, "SEND not supported over UDP\n", strlen("SEND not supported over UDP\n"), 0, (struct sockaddr *)&client, sizeof(client));
        pthread_mutex_unlock(&lock);
        continue;
      }
      else
      {
        printf("Invalid command\n");
        sendto(sock_udp, "Invalid command\n", strlen("Invalid command\n"), 0, (struct sockaddr *)&client, sizeof(client));
      }
      pthread_mutex_unlock(&lock);
    }
  }

  pthread_mutex_destroy(&lock); /* I guess this never happens */
  return (EXIT_SUCCESS);
}

/* Function Declaritions */

int max(int x, int y) 
{ 
  if (x > y) 
    return(x); 
  return(y); 
}

int is_new_userid(char new_userid[20])
{
  for (int i = 0; i < client_index; i++)
  {
    int max_len = max(strlen(clients[i].userid), strlen(new_userid));
    if ((strncmp(clients[i].userid, new_userid, max_len) == 0) && clients[i].active == 1) // if strings are identical and client is active 
    {
      return(0); 
    }
  }
  return(1);
}

int is_alphanumeric(char* word)
{
  int string_length = strlen(word);
  for (int i = 0; i < string_length; i++)
  {
    if(!isalpha(word[i]) && !isdigit(word[i])) // If it isn't a digit or letter
    {
      return(0);
    }
  }
  return(1);
}

void sort_words(char* words[], int count) // https://stackoverflow.com/questions/44982737/sort-word-in-alphabet-order-in-c
{
  char *x;

  for (int i = 0; i < count; i++)
  {
    for (int j = i + 1; j < count; j++)
    {
      if (strcmp(words[i], words[j]) > 0)
      {
        x = words[j];
        words[j] = words[i];
        words[i] = x;
      }
    }
  }
}

void* tcp_receive(void* arg)
{
  #ifdef DEBUG_MODE
    // printf("New Thread Created!\n");
  #endif

  // Don't forget to implement mutexes
  struct client_struct client = *(struct client_struct*) arg; 
  char buffer[BUFFER_SIZE];
  int valid_id = 0; // We haven't checked the ID yet so its invalid 
  int sock = client.socket;  // Get the socket of the client
  int index = client.index; // Index of self in the global array
  clients[index].active = 0; // Be clear that its not active

  int n = 0; 
  while ((n = recv(sock, buffer, BUFFER_SIZE, 0)) >= 0) // Do this until the client terminates
  {
    pthread_mutex_lock(&lock);

    if(n == 0)
    {
      printf("CHILD %d: Client disconnected\n", (int) pthread_self());
      clients[index].active = 0; // Mark not active 
      pthread_mutex_unlock(&lock); // Can't forget this! 
      pthread_exit(NULL);
    }
    buffer[n] = '\0'; /* Not all inputs will be null terminated... */

#if DEBUG_MODE
   // printf("Entire message is: %s\n", buffer);
#endif
    char *token;
    char command[50];
    bzero(command, 50);
    token = strtok(buffer, delimeters);
    strcpy(command, token);

    if (strcmp(command, "LOGIN") == 0)
    {
      char userid[20];
      bzero(userid, 20);
      token = strtok(NULL, delimeters);
      strcpy(userid, token);
      printf("CHILD %d: Rcvd LOGIN request for userid %s\n", (int) pthread_self(), userid);
      if (is_new_userid(userid) == 0)
      {
        #ifdef DEBUG_MODE
          printf("clients[index].active: %d || is_new_userid(userid): %d\n", clients[index].active, is_new_userid(userid));
        #endif
        n = send(sock, "ERROR Already connected\n", strlen("ERROR Already connected\n"), 0);
        printf("CHILD %d: Sent ERROR (Already connected)\n", (int) pthread_self());
        pthread_mutex_unlock(&lock);
        continue;
      }
      else if (strlen(userid) < 4 ||  strlen(userid) > 16 || is_alphanumeric(userid) == 0) 
      {
        n = send(sock, "ERROR Invalid userid\n", strlen("ERROR Invalid userid\n"), 0);
        printf("CHILD %d: Sent ERROR (Invalid userid)\n", (int) pthread_self());
        pthread_mutex_unlock(&lock);
        continue;
      }
      valid_id = 1;
      clients[index].active = 1; 
      strcpy(clients[index].userid, userid);
      n = send(sock, "OK!\n", 4, 0);
    }
    else if(clients[index].active != 1) // Check if logged in 
    {
      printf("Not logged in\n");
    }
    else if (strcmp(command, "WHO") == 0)
    {
      printf("CHILD %d: Rcvd WHO request\n", (int) pthread_self());
      char msg[1024];
      bzero(msg, 1024);
      strcat(msg, "OK!"); // If needed the newline is added in a few lines down
      char **names_sorted = calloc(50, sizeof(char *));
      for (int i = 0; i < 50; i++)
      {
        names_sorted[i] = calloc(50, sizeof(char));
      }
      int num_names = 0;
      for (int i = 0; i < client_index; i++)
      {
        if (clients[i].active == 1)
        {
          strcpy(names_sorted[num_names], clients[i].userid);
          num_names++;
        }
      }

      sort_words(names_sorted, num_names);

      for (int i = 0; i < num_names; i++)
      {
        strcat(msg, "\n");
        strcat(msg, names_sorted[i]);
      }
      strcat(msg, "\n");

#ifdef DEBUG_MODE
      printf("Sending message: %s\n", msg);
#endif
      n = send(sock, msg, strlen(msg), 0);
      for (int i = 0; i < 50; i++)
      {
        free(names_sorted[i]);
      }
      free(names_sorted);
      pthread_mutex_unlock(&lock);
      continue;
    }
    else if (strcmp(command, "LOGOUT") == 0)
    {
      printf("CHILD %d: Rcvd LOGOUT request\n", (int) pthread_self());
      if (valid_id != 1)
      {
        printf("Not logged in\n");
        clients[index].active = 0; // Mark client as inactive 
        pthread_mutex_unlock(&lock);
        continue;
      }
      n = send(sock, "OK!\n", 4, 0);
      clients[index].active = 0; // Whoops forgot to do this. 
      valid_id = 0;
      pthread_mutex_unlock(&lock);
      continue;
    }
    else if (strcmp(command, "SEND") == 0)
    {
      if (valid_id != 1)
      {
        printf("ERROR Not logged in\n");
        pthread_mutex_unlock(&lock);
        continue;
      }

      char recipient_userid[50];
      bzero(recipient_userid, 50);
      int sock_recipient = -1;
      token = strtok(NULL, delimeters);
      strcpy(recipient_userid, token);
      printf("CHILD %d: Rcvd SEND request to userid %s\n", (int) pthread_self(), recipient_userid);
      for (int i = 0; i < client_index; i++)
      {
        if (strcmp(recipient_userid, clients[i].userid) == 0 && clients[i].active == 1)
        {
          sock_recipient = clients[i].socket;
          break;
        }
      }
      if (sock_recipient == -1)
      {
#ifdef DEBUG_MODE
        printf("ERROR (Invalid userid)\n");
#endif
        printf("CHILD %d: Sent ERROR (Unknown userid)\n", (int) pthread_self());
        n = send(sock, "ERROR Unknown userid\n", strlen("ERROR Unknown userid\n"), 0);
        pthread_mutex_unlock(&lock);
        continue;
      }
      char msglenstr[4];
      bzero(msglenstr, 4);
      token = strtok(NULL, delimeters);
      strcpy(msglenstr, token);

      if (strlen(msglenstr) == 0) // If invalid msglen
      {
#ifdef DEBUG_MODE
        printf("Invalid msglen\n");
#endif
        printf("CHILD %d: Sent ERROR Invalid msglen\n", (int) pthread_self());
        n = send(sock, "Sent ERROR Invalid msglen\n", strlen("ERROR Invalid msglen\n"), 0);
        pthread_mutex_unlock(&lock);
        continue;
      }

      int msglen = atoi(msglenstr);
      // Also you need check if the string can be converted to an int, otherwise throw "Invalid SEND format"

      if (msglen < 1 || msglen > 990) // If invalid msglen #2
      {
#ifdef DEBUG_MODE
        // printf("Invalid msglen\n");
#endif
        printf("CHILD %d: Sent ERROR Invalid msglen\n", (int) pthread_self());
        n = send(sock, "Sent ERROR Invalid msglen\n", strlen("ERROR Invalid msglen\n"), 0);
        pthread_mutex_unlock(&lock);
        continue;
      }

      // We should be able to send the message now since we did all the required checks 
      n = send(sock, "OK!\n", 4, 0); // Acknowledge to client who sent the send request 

      token = strtok(NULL, ""); // Get the rest of the mesage, don't split be delimeter. 
      char message[1024];
      bzero(message, 1024);
      strcpy(message, token); // This is the rest of the message
      while(strlen(message) < msglen)
      {
        char rest_of_message[BUFFER_SIZE];
        n = recv(sock, rest_of_message, BUFFER_SIZE, 0);
        strcat(message, rest_of_message);
      }

      char dest[1024];              // Destination string
      bzero(dest, 1024);
      strncpy(dest, message, msglen); // only use msg characters to string
      dest[msglen] = '\0';          // null terminate destination

      char msg_recipient[1024];
      bzero(msg_recipient, 1024);
      strcat(msg_recipient, "FROM ");

      strcat(msg_recipient, clients[index].userid);
      strcat(msg_recipient, " ");

      char msglen_str[10];
      bzero(msglen_str, 10); 
      sprintf(msglen_str, "%d", msglen); // Convert (int) msglen to a str
      strcat(msg_recipient, msglen_str);
      strcat(msg_recipient, " ");

      strcat(msg_recipient, dest); // Copy the actual message and send to client 
      strcat(msg_recipient, "\n");
      #ifdef DEBUG_MODE
        printf("recipient: %s, message: %s\n", recipient_userid, msg_recipient);
      #endif
      send(sock_recipient, msg_recipient, strlen(msg_recipient), 0);
      pthread_mutex_unlock(&lock);
      continue;
    }
    else if (strcmp(command, "BROADCAST") == 0)
    {
      printf("CHILD %d: Rcvd BROADCAST request\n", (int) pthread_self());
      if (valid_id != 1)
      {
        printf("Not logged in\n");
        pthread_mutex_unlock(&lock);
        continue;
      }

      token = strtok(NULL, delimeters);
      int msglen = atoi(token);
      char msglen_str[10];
      bzero(msglen_str, 10);
      sprintf(msglen_str, "%d", msglen);
      char message[1024];
      bzero(message, 1024);
      token = strtok(NULL, ""); // Get the rest of the message
      strcat(message, "FROM ");
      strcat(message, clients[index].userid);
      strcat(message, " "); 
      strcat(message, msglen_str);
      strcat(message, " ");

      while(strlen(token) < msglen)
      {
        char rest_of_message[BUFFER_SIZE];
        bzero(rest_of_message, BUFFER_SIZE);
        n = recv(sock, rest_of_message, BUFFER_SIZE, 0);
        strcat(token, rest_of_message);
      }

      char dest[1024];              // Destination string
      strncpy(dest, token, msglen); // only use msg characters to string
      dest[msglen] = '\0';          // null terminate destination

      strcat(message, dest);
      strcat(message, "\n");
      // Uh... How do I access all the clients in the thread... Do I have to pass them through the struct?
      send(sock, "OK!\n", 4, 0);
      for (int i = 0; i < client_index; i++)
      {
        #ifdef DEBUG_MODE
          // printf("Sending message to client %s: %s\n", clients[i].userid, message);
        #endif
        if (clients[i].active == 1) // Don't broadcast to self
        {
          send(clients[i].socket, message, strlen(message), 0);
          #ifdef DEBUG_MODE
            printf("Message sent to: %s\n", clients[i].userid);
          #endif
        }
      }
      pthread_mutex_unlock(&lock);
      continue;
    }
    else
    {
#ifdef DEBUG_MODE
      printf("Invalid command\n");
#endif
      n = send(sock, "Invalid command\n", strlen("Invalid command\n"), 0);
      pthread_mutex_unlock(&lock);
      continue;
    }
    pthread_mutex_unlock(&lock);
  }
  return(NULL);
}

