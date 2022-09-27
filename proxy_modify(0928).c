/* proxy_part3_cache */

#include <stdio.h>
#include "csapp.h"

static const char *user_agent_header = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_header = "Connection: close\r\n";
static const char *prox_header = "Proxy-Connection: close\r\n";
static const char *host_header_format = "Host: %s\r\n";
static const char *requestlint_header_format = "GET %s HTTP/1.0\r\n";
static const char *endof_header = "\r\n";
static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void cache_init();
void *thread_routine(void *connfdp);
void doit(int connfd);
int parse_uri(char *uri, char *hostname, char *path, int *port);
void makeHTTPheader(char *HTTPheader, char *hostname, char *path, int port, rio_t *client_rio);

int main(int argc, char **argv)
{
    int listenfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    // 캐시 ON
    cache_init(); 
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        int *connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread_routine, connfdp);
    }
    return 0;
}


void *thread_routine(void *connfdp)
{
    int connfd = *((int *)connfdp);
    Pthread_detach(pthread_self());
    Free(connfdp);
    doit(connfd);
    Close(connfd);
}


#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_OBJECT_NUM ((int)(MAX_CACHE_SIZE / MAX_OBJECT_SIZE))

typedef struct                                          // 캐시 구조체
{
    char cache_obj[MAX_OBJECT_SIZE];                    // 요청받은 uri에 해당하는 content
    char cache_uri[MAXLINE];                            // 요청받은 uri
    int order;                                          // 캐시 관리를 위한 우선순위(최근 사용 캐시 기준 내림차순)
    int alloc, read;                                    // 캐시 매핑 여부, 접근 스레드 수
    sem_t sem_w, sem_r;                                 // 세마포어 P, V 연산을 위한 변수
} cache_block;

typedef struct
{
    cache_block cacheOBJ[MAX_OBJECT_NUM];               // 캐시들을 관리하기 위한 구조체
} Cache;

Cache cache;


void cache_init()                                       // cache 구조체 초기화
{
    int index = 0;
    for (index; index < MAX_OBJECT_NUM; index++)
    {
        cache.cacheOBJ[index].order = 0;                // LRU로 구현하기위해 사용될 때마다 갱신해주는 우선순위
        cache.cacheOBJ[index].alloc = 0;                // cache 매핑 여부
        Sem_init(&cache.cacheOBJ[index].sem_w, 0, 1);   // write를 잠그기 위한 sem_w 초기화
        Sem_init(&cache.cacheOBJ[index].sem_r, 0, 1);   // read cnt를 잠그기 위한 sem_r 초기화
        cache.cacheOBJ[index].read = 0;                 // read 중인 스레드 수
    }
}


void readstart(int index)                               // ## P - read할 캐시를 보호 ##
{
    P(&cache.cacheOBJ[index].sem_r);                    // read에 진입하는 스레드 카운트를 (하나씩 정확하게 카운트하기 위해) 보호(wait - read cnt)
    cache.cacheOBJ[index].read++;                    // read하는 스레드가 추가될 때마다 1증가
    
    if (cache.cacheOBJ[index].read == 1)                // 스레드가 읽기 시작하면
        P(&cache.cacheOBJ[index].sem_w);                // write 접근 제한(wait - write)
    V(&cache.cacheOBJ[index].sem_r);                    // 카운트 후 잠금 해제(signal - read cnt)
}


void readend(int index)                                 // ## V - read할 캐시를 보호 ##
{
    P(&cache.cacheOBJ[index].sem_r);                    // read를 끝내는 스레드 카운트를 (하나씩 정확하게 카운트하기 위해) 보호(wait - read cnt)
    cache.cacheOBJ[index].read--;                      // read를 끝내는 스레드가 감소할 때마다 1감소

    if (cache.cacheOBJ[index].read == 0)                // 스레드가 읽기를 마치면
        V(&cache.cacheOBJ[index].sem_w);                // write 접근 허용(signal - write)
    V(&cache.cacheOBJ[index].sem_r);                    // 카운트 후 잠금 해제(signal - read cnt)
}


int cache_find(char *uri)                               // 가용한 캐시가 있는지 탐색하고 있다면 index를 리턴하는 함수
{
    int index = 0;                                      // 가용한 캐시 인덱스를 저장할 변수
    
    for (index; index < MAX_OBJECT_NUM; index++)        // 캐시들을 탐색(10개 다)
    {
        readstart(index);                               // P - read할 캐시를 보호 (wait)
        if (cache.cacheOBJ[index].alloc && (strcmp(uri, 
                cache.cacheOBJ[index].cache_uri) == 0)) // 캐시가 매핑된 상태이고(1) && uri와 캐시의 uri가 같을 때(==0)
            break;                                      // 검색 종료
        readend(index);                                 // V - read할 캐시를 보호 (signal)
    }
    
    if (index == MAX_OBJECT_NUM)                        // index가 끝까지(MAX) 돌았는데 검색이 안될 때 - 캐시 미스
        return -1;                                      // 실패 시 -1 반환
    return index;                                       // 성공 시 idx 반환
}


int cache_add_index()                                   // 새로운 캐시를 추가할 index 결정(LRU)
{
    int minorder = MAX_OBJECT_NUM + 1;                  // 11로 시작해서 우선순위 최솟값으로 갱신될 것
    int minindex = 0;                                   // 우선순위 최솟값인 캐시의 인덱스
    int index = 0;                                      // 검색할 인덱스

    for (index; index < MAX_OBJECT_NUM; index++)        // 캐시들을 탐색(10개 다)
    {
        readstart(index);                               // P - read할 캐시를 보호 (wait)

        if (!cache.cacheOBJ[index].alloc)               // 캐시가 매핑되지않은 빈 상태이면(0)
        {
            readend(index);                             // V - read할 캐시를 보호 (signal)
            return index;                               // index 반환
        }

        if (cache.cacheOBJ[index].order < minorder)     // minorder보다 작은 애들을 비교하면서
        {
            minindex = index;                           // 최솟값의 인덱스로 갱신
            minorder = cache.cacheOBJ[index].order;     // 최솟값으로 갱신
        }
    readend(index);                                     // V - read할 캐시를 보호 (signal)
    }
    
    return minindex;                                    // minindex 반환 -> LRU
}


void cache_reorder(int target)                          // 추가한 캐시의 우선순위 갱신
{
    cache.cacheOBJ[target].order = MAX_OBJECT_NUM + 1;  // 사용한 캐시의 우선순위 11로 갱신
    int index = 0;                                      // 검색할 인덱스
    for (index; index < MAX_OBJECT_NUM; index++)        // 캐시들을 탐색(10개 다)
    {
        if (index != target)                            // 사용한 캐시(자기자리) 빼고 다
        {                                               // 수정할거니까 write 보호
            P(&cache.cacheOBJ[index].sem_w);            // P - write 접근 제한(wait - write)
            cache.cacheOBJ[index].order--;              // 사용한 캐시 제외하고 우선순위 -1
            V(&cache.cacheOBJ[index].sem_w);            // V - write 접근 허용(signal - write)
        }
    }
}


void cache_uri(char *uri, char *buf)                    // 새로운 캐시에 데이터 저장
{
    int index = cache_add_index();                      // 새로운 캐시를 추가할 index 결정(LRU)

    P(&cache.cacheOBJ[index].sem_w);                    // P - write 접근 제한(wait - write)

    strcpy(cache.cacheOBJ[index].cache_obj, buf);       // buf 내용 복사
    strcpy(cache.cacheOBJ[index].cache_uri, uri);       // uri 내용 복사
    cache.cacheOBJ[index].alloc = 1;                    // 매핑 여부 1로 갱신     

    cache_reorder(index);                               // 추가한 캐시의 우선순위 갱신

    V(&cache.cacheOBJ[index].sem_w);                    // V - write 접근 허용(signal - write)
}


void doit(int connfd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char HTTPheader[MAXLINE], hostname[MAXLINE], path[MAXLINE];
    int backfd;
    rio_t rio, backrio;
    
    Rio_readinitb(&rio, connfd);                
    Rio_readlineb(&rio, buf, MAXLINE);          
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET"))
    {
        printf("Proxy does not implement this method\n");
        return;
    }

    char uri_store[MAX_OBJECT_SIZE];
    strcpy(uri_store, uri);
    int cache_index;
    if ((cache_index = cache_find(uri_store)) != -1)    // 캐시에 요청받은 uri가 있는지 확인
    {                                                   // 캐시 적중 - 캐시 내용 전송 후 return
        readstart(cache_index);                         // P - read할 캐시를 보호 (wait)
        Rio_writen(connfd,                              // 캐시의 내용을 connfd에 전송
            cache.cacheOBJ[cache_index].cache_obj, 
            strlen(cache.cacheOBJ[cache_index].cache_obj));
        readend(cache_index);                           // V - read할 캐시를 보호 (signal)
        return;
    }
    int port;
    parse_uri(uri, hostname, path, &port);
    makeHTTPheader(HTTPheader, hostname, path, port, &rio);

    char portch[10];
    sprintf(portch, "%d", port);
    backfd = Open_clientfd(hostname, portch);
    if(backfd < 0)
    {
        printf("connection failed\n");
        return;
    }
    Rio_readinitb(&backrio, backfd);
    Rio_writen(backfd, HTTPheader, strlen(HTTPheader));
    char cachebuf[MAX_OBJECT_SIZE];
    size_t sizerecvd, sizebuf = 0;
    while((sizerecvd = Rio_readlineb(&backrio, buf, MAXLINE)) != 0)
    {
        sizebuf = sizebuf + sizerecvd;
        if (sizebuf < MAX_OBJECT_SIZE)
        {
            strcat(cachebuf, buf);
        }
        printf("proxy received %d bytes, then send\n", sizerecvd);
        Rio_writen(connfd, buf, sizerecvd);
    }
    Close(backfd);
    if (sizebuf < MAX_OBJECT_SIZE)
    {
        cache_uri(uri_store, cachebuf);                 // 캐시 미스 - 새로운 캐시 추가
    }
}


int parse_uri(char *uri, char *hostname, char *path, int *port)
{
    *port = 80;
    char *hostnameP = strstr(uri, "//");
    if (hostnameP != NULL)
    {
        hostnameP = hostnameP + 2;
    }
    else
    {
        hostnameP = uri;
    }

    char *pathP = strstr(hostnameP, ":");
    if(pathP != NULL)
    {
        *pathP = '\0';
        sscanf(hostnameP, "%s", hostname);
        sscanf(pathP + 1, "%d%s", port, path);
    }
    else
    {
        pathP = strstr(hostnameP, "/");
        if(pathP != NULL)
        {
            *pathP = '\0';
            sscanf(hostnameP, "%s", hostname);
            *pathP = '/';
            sscanf(pathP, "%s", path);
        }
        else
        {
            sscanf(hostnameP, "%s", hostname);
        }
    }
    return 0;
}


void makeHTTPheader(char *HTTPheader, char *hostname, char *path, int port, rio_t *client_rio)
{
    char buf[MAXLINE], request_header[MAXLINE], other_header[MAXLINE], host_header[MAXLINE];
    sprintf(request_header, requestlint_header_format, path);
    while(Rio_readlineb(client_rio, buf, MAXLINE) > 0)
    {
        if(strcmp(buf, endof_header) == 0)
        {
            break;
        }
        if(!strncasecmp(buf, host_key, strlen(host_key)))
        {
            strcpy(host_header, buf);
            continue;
        }
        if(!strncasecmp(buf, connection_key, strlen(connection_key))
                &&!strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
                &&!strncasecmp(buf, user_agent_key, strlen(user_agent_key)))
        {
            strcat(other_header, buf);
        }
    }
    if(strlen(host_header) == 0)
    {
        sprintf(host_header, host_header_format, hostname);
    }
    sprintf(HTTPheader, "%s%s%s%s%s%s%s", request_header, host_header, conn_header, prox_header, user_agent_header, other_header, endof_header);
}