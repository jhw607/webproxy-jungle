#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv){
    // argc : (공백을 기준으로 나눈) 입력 개수 / argv : 배열처럼 입력값을 참조(0부터)
    int listenfd, connfd;   // 듣기식별자와 연결식별자
    socklen_t clientlen;    // 클라이언트 길이?
    struct sockaddr_storage clientaddr; // 
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if(argc != 2){
        fprintf(stderr, "usage : %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);
        Close(connfd);        
    }
    exit(0);
}