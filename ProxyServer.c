#include "proxy.h"

//Function prototypes.
void *connection_handler(void *);

int main(int argc , char *argv[]) {
  struct sockaddr_in server, client;
  pthread_t sniffer_thread;
  int server_socket, client_socket;
  int addr_size = sizeof(struct sockaddr_in);
  int PORT = atoi(argv[1]);
  int *new_sock;
  char *message;

  //Create a socket on the server.
  if ((server_socket = socket(AF_INET , SOCK_STREAM , 0)) < 0) {
    printf("Error! socket() failed.\n");
    exit(1);
  }

  //Prepare the sockaddr_in structure
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(PORT);

  if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) < 0) {
      printf("Error! bind() failed.\n");
      exit(1);
  }

  //Listen for an incoming client connection (5 maximum).
  listen(server_socket, 5);

  printf("Server established!\n");
  printf("Waiting for a request...\n");

  //Change to while(1).
  while(1) {
    if ((client_socket = accept(server_socket, (struct sockaddr *)&client, &addr_size)) < 0) {
      printf("Error! accept() failed.\n");
      close(server_socket);
      exit(1);
    }

    else {
      printf("IP address %s ", (char*)inet_ntoa(client.sin_addr));
  		printf("is connected on port %d\n", htons(client.sin_port));

      new_sock = malloc(1);
      *new_sock = client_socket;

      if(pthread_create(&sniffer_thread, NULL,  connection_handler, (void*)new_sock) < 0) {
          printf("Error! Failed to create a thread.\n");
          close(client_socket);
          close(server_socket);
          free(new_sock);
          exit(1);
      }
    }
    printf("Waiting for a request...\n");
  }
  return 0;
}

//This will handle connection for each client
void *connection_handler(void *desc) {
  struct sockaddr_in client_addr;
  int addrlen = sizeof(client_addr);
  int ClientSocket = *(int*)desc;
  int msg_size;
  char buffer[MAXBUF];
  char url[MAXBUF], filename[MAXBUF], bad_request[MAXBUF], OK[MAXBUF];
  char *temp = NULL;
  char line[8096];
  size_t len = 0;
  char *filter = NULL;
  FILE* site;

  //Build the OK response
  strcpy(OK, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");

  //Recieve the client message.
  msg_size = recv(ClientSocket, buffer, MAXBUF, 0);

  //DEGBUG:
  //printf("\nClient Request:\n%s",buffer);

  temp = strtok(buffer," ");
  temp = strtok(NULL,"/ ");
  strcpy(url, temp);
  temp = NULL;
  //DEBUG: Should have "[url]".
  //printf("Parsed URL: %s\n", url);

  //Blacklist filter.
  site = fopen("blacklist", "r");
  while (getline(&filter, &len, site) != -1) {
    filter[strlen(filter)-1]='\0';

    temp = (char*)strcasestr(url, filter);

    if (temp != NULL) {
      printf("The domain '%s' is blacklisted!\n", url);

      //Build and send the 404 error response message.
      strcpy(bad_request, "HTTP/1.1 404 sendErrorErrorError\r\n");
      strcat(bad_request, "Content-Type: text/html\r\n\r\n<html><h1>AWWWW DAMN. THAT SHITE IS BLACKLISTED!</h1></html>\n");
      send(ClientSocket, bad_request, strlen(bad_request), 0);
      close(ClientSocket);
      free(desc);
      pthread_exit("Exiting...\n");
      return;
    }
    temp = NULL;
  }
  fclose(site);

  //There is a cache file found for the website.
  if (doesFileExist(url)) {
    printf("Reading from cache...\n");
    site = fopen(url,"r"); //solo read semaphore?

    //Remove extraneous lines from the file buffer.
    while (fgets(line, 8096, site) != NULL) {
      if (line[0] == '<' || line[1] == '<')
        break;
    }

    //Send the OK message.
    send(ClientSocket, OK, sizeof(OK), 0);

    //Send the file buffer data to the client.
    while (1) {
      send(ClientSocket, line, sizeof(line), 0);
      bzero(line, sizeof(line));

      if (fgets(line, 8096, site) == NULL)
        break;

      strcpy(line, lang_filter(line, "swears"));
    }
    fclose(site);
  }
  //File does not exist, get the html!
  else {
    //printf("No cache found for '%s', fetching html...\n", url);
    getHTML(url, ClientSocket);
  }

  //Close data connections and free memory.
  close(ClientSocket);
  free(desc);
  pthread_exit("Exiting...\n");
}
