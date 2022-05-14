#include "csapp.h"

struct  sockaddr_in{
    uint16_t sin_family;            // AF_INET(2byte)
    uint16_t sin_port;              // 16비트 포트 번호(2byte)
    struct  in_addr sin_addr;       // 32비트 IP 주소(4byte)
    unsigned char sin_zero[8];      // (8byte)
};

struct sockaddr{
    uint16_t sa_family;             // protocol family  
    char sa_data[14];               // address data
};

int socket(int domain, int type, int protocol);
    // return : 성공 시 비음수 식별자 or 에러 시 -1 

// clientfd = socket(AF_INET, SOCK_STREAM, 0);
    // AF_INET : IPv4
    // SOCK_STREAM : TCP

int connect(int clientfd, const struct sockaddr *addr, socklen_t addrlen);
    // return : 성공 시 0, 에러 시 -1

// (x:y, addr.sin_addr:addr.sin_port)

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
    // return : 성공 시 0, 에러 시 -1

int listen(int sockfd, int backlog);   
    // 듣기식별자로 변환
    // return : 성공 시 0, 에러 시 -1

int accept(int listenfd, struct sockaddr *addr, int *addrlen);
    // 클라이언트로부터 연결요청을 기다림
    // return : 성공 시 비음수 연결식별자 or 에러 시 -1 


    