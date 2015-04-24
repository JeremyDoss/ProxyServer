#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <regex.h> //remove bad words

#define MAXBUF 1024

int doesFileExist(char *filename) {
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

char* lang_filter(char* buffer, char* fname) {
  FILE* words;
  char *filter = NULL;
  char *pos = NULL;
  size_t len = 0;
  int i;

  words = fopen(fname,"r");
  while (getline(&filter, &len, words) != -1) {
    filter[strlen(filter)-1]='\0';

    while(1) {
      pos = (char*)strcasestr(buffer, filter);

      if (pos == NULL)
        break;
      //else
        //printf("profanity found!\n");

      for(i = 0; i < strlen(filter); i++)
        pos[i] = '*';

      pos = NULL;
    }
  }
  fclose(words);
  return buffer;
}

void getHTML(char* Hostname, int ClientSocket) {
  struct addrinfo hints, *res;
  int sockfd, msg_length, s = 0;
  char buffer[MAXBUF], filtered[MAXBUF], request[MAXBUF], bad_request[MAXBUF];
  FILE* cache_file;

  //Build the clients HTTP request.
  strcpy(request, "GET / HTTP/1.1\r\nHost: ");
  strcat(request, Hostname);
  strcat(request, ":80\r\nConnection: keep-alive\r\n\r\n");

  //DEBUG:
  printf("Hostname: %s\n", Hostname);

  //NEED TO REMOVE "WWW." FROM THE HOST NAME!!!!

  //for object retreival
  //if (.com/.net/.org/.edu is not in the string) dont write file, duplicate and return request

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((s = getaddrinfo(Hostname, "80", &hints, &res)) != 0) {
    printf("Error! %s\n", gai_strerror(s));
    close(sockfd);
    return;
  }

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
    printf("Couldn't establish a socket.\n");
    close(sockfd);
    return;
  }

  if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
    //Send request to the Hostname
    //printf("Outbound request:\n%s", request);
    send(sockfd, request, strlen(request),0);

	//TRY!!!!!!!!!!!!!!!!!!!!!!!!!!
	bzero(request, sizeof(request));
	//bzero(buffer, sizeof(buffer));

    cache_file = fopen(Hostname, "a");
    //Send the response to the client.
    while(1) {
      //Clear the buffer.
      bzero(buffer, sizeof(buffer));
      if ((msg_length = recv(sockfd, buffer, MAXBUF,0)) <= 0)
        break;

      //Profanity filter.
      strcpy(buffer, lang_filter(buffer, "swears"));
      //printf("Filtered: %s", buffer);

      //Write to file.
      fprintf(cache_file,"%s",buffer);
      //Send line to client.
      send(ClientSocket, buffer, msg_length, 0);
  	}
    fclose(cache_file);
  }

  else {
    printf("Host Connection: %s\n", strerror(errno));
    //Build and send the 404 error response message.
    strcpy(bad_request, "HTTP/1.1 404 sendErrorErrorError\r\n");
    strcat(bad_request, "Content-Type:text/html\r\n\r\n<html><h1>NOT FOUND!</h1></html>");
    send(ClientSocket, bad_request, strlen(bad_request), 0);
  }
  close(sockfd);
}
