#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXLINE 72
//#define FILEMAX 5
#define FILEMAX 16200
#define SETMSGLEN 72
#define CALMSGLEN 72
#define USLEEP 5000

short int setflag = 0;
short int calflag = 1;
short int retflag = 2;
short int resultflag = 3;
short int shutdownflag = 4;

int shutdownstat = 0;
int nodeip[10];
short int nodeport[10];
int myserverfd, myserverconnfd; //他ノードから受け取る(server)
int baseip;
short int baseport;
short int myid = 0;
int baseconnfd; //基本ノードへ接続

struct SETMSG
{
    short int flag;
    short int myid;
    int baseip;
    short int baseport;
    int ip[10];
    short int port[10];
};

struct CALMSG
{
    short int flag;
    short int fileno;
    int count;
    float result;
    int ip[8];
    short int port[8];
};

struct MSGTHREAD
{
    char msg[SETMSGLEN];
    int no; // 1-10
    int connfd;
};

void *msgProcessBase(void *arg);
void *msgProcess(void *arg);
float cal(float lastresult);
void printCALMSG(struct CALMSG *a);
void sendNode(int ip, short int port, struct CALMSG *caltemp);
void calMSG(struct CALMSG *caltemp);
ssize_t readn(int fd, void *buf, size_t count);
ssize_t writen(int fd, void *buf, size_t count);
ssize_t writen(int fd, void *buf, size_t count)
{
    int left = count;
    char *ptr = (char *)buf;
    while (left > 0)
    {
        int writeBytes = write(fd, ptr, left);
        if (writeBytes < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        else if (writeBytes == 0)
            continue;
        left -= writeBytes;
        ptr += writeBytes;
    }
    return count;
}
ssize_t readn(int fd, void *buf, size_t count)
{
    int left = count; //剩下的字节
    char *ptr = (char *)buf;
    while (left > 0)
    {
        int readBytes = read(fd, ptr, left);
        if (readBytes < 0) //read函数小于0有两种情况：1中断 2出错
        {
            if (errno == EINTR) //读被中断
            {
                continue;
            }
            return -1;
        }
        else if (readBytes == 0)
        {
            return count - left;
        }
        //printf("readBytes: %d",readBytes);
        left -= readBytes;
        ptr += readBytes;
    }
    return count;
}
void calMSG(struct CALMSG *calt)
{
    struct CALMSG caltemp = *calt;
    struct CALMSG sendtemp = {0};
    char sendline[MAXLINE];
    memset(sendline, 0, MAXLINE * sizeof(char));
    int ret = 0;
    if (caltemp.flag == calflag)
    {
        if (caltemp.count == 2)
        { //base に返す
            sendtemp.flag = retflag;
        }
        else
        {
            sendtemp.flag = calflag;
        }
        sendtemp.count = caltemp.count - 1;
        sendtemp.fileno = caltemp.fileno;
        sendtemp.result = cal(caltemp.result);

        for (int i = 0; i < 7; i++)
        {
            sendtemp.ip[i] = caltemp.ip[i + 1];
            sendtemp.port[i] = caltemp.port[i + 1];
        }
        //recvline = *(char *)(&sendtemp);
        //次のノードに接続
        //puts("sendtemp:");
        //printCALMSG(&sendtemp);
        sendNode(sendtemp.ip[0], sendtemp.port[0], &sendtemp);
    }
    else if (caltemp.flag == retflag)
    {
        //一回計算し、結果をbaseに
        sendtemp.result = cal(caltemp.result);
        sendtemp.count = caltemp.count - 1;
        sendtemp.flag = resultflag;
        sendtemp.fileno = caltemp.fileno;

        *(struct CALMSG *)sendline = sendtemp;

        sendNode(baseip, baseport, &sendtemp);
        //printf("send:no=%d, from %d to base\n", (caltemp).fileno, myid);
        //perror("send base");
    }
    else if (caltemp.flag == shutdownflag)
    {
        printf("shutdown %d\n", myid);
    }
}

void sendNode(int ip, short int port, struct CALMSG *caltemp)
{
    int sendfd;
    int ret;
    char sendline[MAXLINE];
    memset(sendline, 0, MAXLINE * sizeof(char));

    *(struct CALMSG *)sendline = *caltemp;

    sendfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in tempaddr;
    memset(&tempaddr, 0, sizeof(tempaddr));
    tempaddr.sin_family = AF_INET;
    //ポート60211~60220
    tempaddr.sin_addr.s_addr = ip;
    tempaddr.sin_port = port;
    ret = 1;
    //printf("conn to: %x(%d)\n", *(int *)&tempaddr.sin_addr.s_addr, ntohs(tempaddr.sin_port));
    //成功 ret=0
    while (ret != 0)
    {
        ret = connect(sendfd, (struct sockaddr *)&tempaddr, sizeof(tempaddr));
        //perror("connect by base");
    }
    ret = -1;
    while (ret <= 0)
    {
        ret = writen(sendfd, sendline, CALMSGLEN);
        //printf("ret=%d,send to %d\n", ret, ntohs(port));
        //perror("send to ");
    }
    //printf("send:no=%d, from %d to %d\n", (*caltemp).fileno, myid, ntohs(port));

    //printf("sent to   ");
    close(sendfd);
}

void printCALMSG(struct CALMSG *a)
{
    printf("flag:%hd, fileno:%hd\n", (*a).flag, (*a).fileno);
    printf("count:%d, result:%f \n", (*a).count, (*a).result);
    for (int i = 0; i < 8; i++)
    {
        printf("[%d] %x:%hu\n", i, (*a).ip[i], ntohs((*a).port[i]));
    }
}
float cal(float lastresult)
{
    float result = 0;
    switch (myid)
    {
    case 1:
        result = lastresult + 1;
        break;
    case 2:
        result = lastresult / 2;
        break;
    case 3:
        result = lastresult * 3;
        break;
    case 4:
        result = lastresult - 4;
        break;
    case 5:
        result = lastresult / 5;
        break;
    case 6:
        result = lastresult + 6;
        break;
    case 7:
        result = lastresult - 7;
        break;
    case 8:
        result = lastresult * 8;
        break;
    case 9:
        result = lastresult - 9;
        break;
    case 10:
        result = lastresult * 10;
        break;
    default:
        printf("wrong");
        //return 1;
        break;
    }
    return result;
}
void *msgProcessBase(void *arg)
{
    struct MSGTHREAD *t;
    t = (struct MSGTHREAD *)arg;
    int ret;
    char recvline[MAXLINE];
    memset(recvline, 0, MAXLINE * sizeof(char));
    struct sockaddr_in nodeaddr = {0};
    while (ret <= 0)
    {
        ret = readn(baseconnfd, recvline, SETMSGLEN);
    }
    /***settemp msg received***/
    struct SETMSG settemp = {0};
    settemp = *(struct SETMSG *)recvline;
    if (settemp.flag == setflag)
    {
        printf("my ID is:%u ret:%d\n", settemp.myid, ret);
        myid = settemp.myid;
        baseip = settemp.baseip;
        baseport = settemp.baseport;
        for (int i = 0; i < 10; i++) //(0-10)
        {
            nodeip[i] = *(in_addr_t *)&(settemp.ip[i]);
            nodeport[i] = htons(*(short *)&(settemp.port[i]));
        }
    }
    else
    {
        printf("error flag=%d", settemp.flag);
    }
    /***cal msg received***/
    while (1)
    {
        memset(recvline, 0, MAXLINE * sizeof(char));
        ret = 0;
        while (ret <= 0)
        {
            ret = readn(baseconnfd, recvline, CALMSGLEN);
        }
        //printf("recvfromPB %d\n", ret);
        struct CALMSG caltemp = {0};
        struct CALMSG sendtemp = {0};
        //printf("recvfromPB %x\n", recvline);
        //memcpy(&caltemp, recvline, sizeof(caltemp));

        caltemp = *(struct CALMSG *)recvline;

        //puts("caltemp:");
        //printCALMSG(&caltemp);
        calMSG(&caltemp);
        if (caltemp.flag == shutdownflag)
        {
            printf("shutdown %d\n", myid);
            close(baseconnfd);
            close(myserverconnfd);
            close(myserverfd);
            shutdownstat = 1;

            pthread_exit(0);
            break;
        }
    }
}
void *msgProcess(void *arg)
{
    //printf("pthread msgProcess start\n");

    struct MSGTHREAD *t;
    t = (struct MSGTHREAD *)arg;
    int ret;
    char recvline[MAXLINE];
    memset(recvline, 0, MAXLINE * sizeof(char));
    ret = 0;
    while (ret <= 0)
    {
        ret = readn((*t).connfd, recvline, CALMSGLEN);
    }
    struct CALMSG caltemp = {0};
    struct CALMSG sendtemp = {0};
    caltemp = *(struct CALMSG *)recvline;
    //printf("recv:no=%d, from ?to %d\n", (caltemp).fileno, myid);
    //puts("caltemp:");
    //printCALMSG(&caltemp);
    calMSG(&caltemp);
    close((*t).connfd);
}

int main(int argc, char *argv[])
{
    int reuse = 1;
    int PORTNUM = 60200;
    int myid;
    int ret = 0;
    struct sockaddr_in nodeaddr = {0};

    pthread_t basethread;
    //pthread_t nodethread[8];

    char recvline[MAXLINE], sendline[MAXLINE];
    memset(recvline, 0, MAXLINE * sizeof(char));
    memset(sendline, 0, MAXLINE * sizeof(char));

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    if (argc != 2)
    {
        printf("need port.\n");
        //return 1;
    }
    //手入力で基本ノードip port
    servaddr.sin_family = AF_INET;
    //servaddr.sin_port = htons(PORTNUM);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ((servaddr.sin_port = htons(atoi(argv[1]))) == 0)
    {
        printf("wrong port.\n");
        //return 1;
    }
    printf("my addr %s:%d\n", inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

    //他ノードのportとIP設定

    //SERVERを立てる
    if ((myserverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("myserverfd socket");
        //return 1;
    }

    setsockopt(myserverfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    if (bind(myserverfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) < 0)
    {
        perror("myserverfd bind ");
        //return 1;
    }
    if (listen(myserverfd, 256) < 0)
    {
        perror("myserverfd listen");
        //return 1;
    }

    //初めては必ずbaseへ接続
    struct sockaddr_in baddr = {0};
    int blen = sizeof(baddr);

    if ((baseconnfd = accept(myserverfd, (struct sockaddr *)&baddr, &blen)) < 0)
    {
        perror("accept");
        //return 1;
    }
    else
    {
        printf("======connected to base======\n");
        struct MSGTHREAD t = {0};
        t.connfd = baseconnfd;
        pthread_create(&basethread, NULL, msgProcessBase, (void *)&t);
        pthread_detach(basethread);
    }

    //他ノードから計算要求を受け取る
    struct sockaddr_in caddr = {0};
    int clen;
    int temp = 0;

    while (1)
    {
        clen = sizeof(caddr);
        //printf("t %d \n", temp);
        if ((myserverconnfd = accept(myserverfd, (struct sockaddr *)&caddr, &clen)) < 0)
        {
            perror("accept");
            //break;
        }
        else
        {
            pthread_t nodethread;
            struct MSGTHREAD t = {0};
            t.connfd = myserverconnfd;
            pthread_create(&nodethread, NULL, msgProcess, (void *)&t);
            pthread_detach(nodethread);
            temp++;
        }
        if (shutdownstat == 1)
        {
            return 0;
        }
        usleep(USLEEP);

        temp = temp % 8;
    }
}