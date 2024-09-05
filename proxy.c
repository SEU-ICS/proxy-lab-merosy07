#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void *thread(void *);
void doit(int client_fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void build_new_request_hdr(rio_t *rio_packet, char *new_request, char *hostname, char *port);

int main(int argc, char **argv) {
    int listenfd, *connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* Check command line args */
    if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
		clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
		*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
    	Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
//      doit(connfd); Close(connfd);  
        Pthread_create(&tid, NULL, thread, connfd);                                                                       
    }
}


void *thread(void *ptr){
    int connfd = *((int *)ptr);
    Pthread_detach(pthread_self());
    doit(connfd);
    Free(ptr);
    Close(connfd);
    return;
}


void doit(int client_fd) {
    int real_server_fd, port, len;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE], path[MAXLINE];
    rio_t real_client, real_server;

    Rio_readinitb(&real_client, client_fd);
    if (!Rio_readlineb(&real_client, buf, MAXLINE))  	 
        return;
    sscanf(buf, "%s %s %s", method, uri, version);       
    if (strcasecmp(method, "GET")) {                     
        clienterror(client_fd, method, "501", "Not Implemented","Tiny does not implement this method");
        return;
    }                                                    
    
    parse_uri(uri, hostname, path, &port);
    char port_str[10];
    sprintf(port_str, "%d", port); 
    real_server_fd = Open_clientfd(hostname, port_str);  
	if(real_server_fd < 0){
        printf("connection failed\n");
        return;
    }
    Rio_readinitb(&real_server, real_server_fd);
    
    char new_request[MAXLINE];
    sprintf(new_request, "GET %s HTTP/1.0\r\n", path);
    build_new_request_hdr(&real_client, new_request, hostname, port_str);

    Rio_writen(real_server_fd, new_request, strlen(new_request));
    
    while((len = Rio_readlineb(&real_server, buf, MAXLINE)))
        Rio_writen(client_fd, buf, len);
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];
    
    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}


void parse_uri(char *uri, char *hostname, char *path, int *port) {
    *port = 80; 
    char* hostname_ptr = strstr(uri,"//");
    if (hostname_ptr) 
        hostname_ptr += 2; 
    else
        hostname_ptr = uri; 
    
    char* port_ptr = strstr(hostname_ptr, ":"); 
    if (port_ptr) {
        *port_ptr = '\0'; 
        strncpy(hostname, hostname_ptr, MAXLINE);
        sscanf(port_ptr + 1,"%d%s", port, path); 
    } 
    else {
        char* path_ptr = strstr(hostname_ptr,"/");
        if (path_ptr) {
            *path_ptr = '\0';
            strncpy(hostname, hostname_ptr, MAXLINE);
            *path_ptr = '/';
            strncpy(path, path_ptr, MAXLINE);
            return;                               
        }
        strncpy(hostname, hostname_ptr, MAXLINE);
        strcpy(path,"");
    }
    return;
}


void build_new_request_hdr(rio_t *real_client, char *new_request, char *hostname, char *port){
    char temp_buf[MAXLINE];

    while(Rio_readlineb(real_client, temp_buf, MAXLINE) > 0){
        if (strstr(temp_buf, "\r\n")) break; 
        if (strstr(temp_buf, "Host:")) continue;
        if (strstr(temp_buf, "User-Agent:")) continue;
        if (strstr(temp_buf, "Connection:")) continue;
        if (strstr(temp_buf, "Proxy Connection:")) continue;

        sprintf(new_request, "%s%s", new_request, temp_buf);
    }

    sprintf(new_request, "%sHost: %s:%s\r\n", new_request, hostname, port);
    sprintf(new_request, "%s%s%s%s", new_request, user_agent_hdr, "Connection: close\r\n", "Proxy-Connection: close\r\n");
    sprintf(new_request,"%s\r\n", new_request);
}
