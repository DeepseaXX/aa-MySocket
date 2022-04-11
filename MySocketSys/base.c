#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAXLINE 72
//#define FILEMAX 5
#define FILEMAX 16200
#define SETMSGLEN 72
#define CALMSGLEN 72
#define RESULTSIZE 16300
#define USLEEP 5000
int resultcount = 0;
short int setflag = 0;
short int calflag = 1;
short int retflag = 2;
short int resultflag = 3;
short int shutdownflag = 4;
struct sockaddr_in baseaddr = {0};
int clientconnfd[10];
int nodeip[10];
short int nodeport[10];
int servip;
short int servport;
float results[RESULTSIZE] = {0};
pthread_mutex_t mutex;
pthread_t recvthread;
pthread_t nodethread[10];
pthread_t servthread;
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
struct SETNODE
{
    char msg[SETMSGLEN];
    int no; // 1-10
    int connfd;
};
void *setNode(void *arg);
void printCALMSG(struct CALMSG *a);
void *resultRecv(void *arg);
void *serv(void *arg);
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
        //printf("readBytes: %d", readBytes);
        left -= readBytes;
        ptr += readBytes;
    }
    return count;
}
void *serv(void *arg)
{
    puts("======serv waiting for results======");
    struct SETNODE t = *(struct SETNODE *)arg;

    //struct timeval tv;
    int ret;
    char sendline[MAXLINE];
    memset(sendline, 0, sizeof(sendline));
    int servfd, servconnfd;
    int reuse = 1;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = servport;
    servaddr.sin_addr.s_addr = servip;

    if ((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("servfd socket");
    }
    setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    if (bind(servfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) < 0)
    {
        perror("servfd bind ");
    }
    if (listen(servfd, 256) < 0)
    {
        perror("servfd listen");
    }
    struct sockaddr_in baddr = {0};
    int blen = sizeof(baddr);

    while (1)
    {
        if ((servconnfd = accept(servfd, (struct sockaddr *)&baddr, &blen)) < 0)
        {
            perror("accept");
        }
        else
        {
            //printf("======connected from node======\n");
            struct SETNODE t = {0};
            t.connfd = servconnfd;
            pthread_create(&recvthread, NULL, resultRecv, (void *)&t);
            pthread_join(recvthread, 0);
        }
        usleep(USLEEP);
    }
}

void *resultRecv(void *arg)
{
    //puts("======resultRecv======");
    char recvline[MAXLINE];
    memset(recvline, 0, MAXLINE * sizeof(char));
    struct SETNODE *t;
    t = (struct SETNODE *)arg;
    int ret;

    ret = -1;
    while (ret <= 0)
    {
        ret = readn((*t).connfd, recvline, CALMSGLEN);
    }

    struct CALMSG caltemp = {0};
    caltemp = *(struct CALMSG *)recvline;
    results[caltemp.fileno] = caltemp.result;
    pthread_mutex_lock(&mutex);
    resultcount++;
    //printf("ret:%d count:%d no:%d result:%f\n", ret, resultcount, caltemp.fileno, caltemp.result);
    pthread_mutex_unlock(&mutex);
    ret = 0;
    close((*t).connfd);
    return 0;
    //printCALMSG(&t);
    //printf("recv:%d\n", resultcount);
}

void printCALMSG(struct CALMSG *a)
{
    printf("flag:%hd, fileno:%hd\n", (*a).flag, (*a).fileno);
    printf("count:%d, result:%f \n", (*a).count, (*a).result);
    for (int i = 0; i < 8; i++)
    {
        printf("[%d] %x:%d\n", i, (*a).ip[i], ntohs((*a).port[i]));
    }
}
void *setNode(void *arg)
{
    char recvline[MAXLINE];
    struct SETNODE *thisnode;
    thisnode = (struct SETNODE *)arg;
    int ret;
    ret = -1;
    while (ret <= 0)
    {
        ret = writen((*thisnode).connfd, (*thisnode).msg, SETMSGLEN);
    }
    printf("send settemp msg to node: %d %x %x\n", (*thisnode).no, (*thisnode).connfd, (*thisnode).msg);

    char sendline[MAXLINE];
    memset(sendline, 0, sizeof(sendline));
    return 0;
}

int main(int argc, char **argv)
{
    int serverfd, serverconnfd;
    int PORTNUM = 60200;
    int ret;
    struct sockaddr_in baseaddr;
    //    struct sockaddr_in nodeaddr;
    pthread_mutex_init(&mutex, 0);

    char setmsg[MAXLINE];
    char calmsg[MAXLINE];
    memset(setmsg, 0, MAXLINE * sizeof(char));
    memset(calmsg, 0, MAXLINE * sizeof(char));

    struct timeval tv_begin, tv_end;
    //clientconnfd 0-9
    //nodeaddr 0-9
    //id 1-10
    //sockconnected 0-9
    //thread 0-9
    //portとIP設定

    memset(&baseaddr, 0, sizeof(baseaddr));
    baseaddr.sin_family = AF_INET;
    //ポート60211~60220
    servip = inet_addr("127.0.0.1");
    servport = htons(PORTNUM);
    baseaddr.sin_addr.s_addr = servip;
    baseaddr.sin_port = servport;

    for (int i = 0; i < 10; i++)
    {
        nodeip[i] = inet_addr("127.0.0.1");
        nodeport[i] = htons(PORTNUM + 2 * i + 1);
    }

    /***settemp msg***/
    struct SETMSG settemp = {0};
    settemp.baseip = servip;
    settemp.baseport = servport;
    settemp.flag = setflag;
    for (int i = 0; i < 10; i++)
    {
        settemp.ip[i] = nodeip[i];
        settemp.port[i] = nodeport[i];
    }
    printf("======waiting for nodes' request======\n");
    struct sockaddr_in caddr = {0};
    int clen;
    int sockconnected = 0;
    struct SETNODE sendt = {0};

    while (sockconnected < 10)
    {
        //sockconnected 0-9
        //1から10の端末に接続
        clientconnfd[sockconnected] = socket(AF_INET, SOCK_STREAM, 0);
        ret = -1;
        //printf("waiting %s:%d\n", inet_ntoa(nodeaddr[sockconnected].sin_addr), ntohs(nodeaddr[sockconnected].sin_port));
        struct sockaddr_in nodeaddr;

        memset(&nodeaddr, 0, sizeof(nodeaddr));
        nodeaddr.sin_family = AF_INET;
        nodeaddr.sin_addr.s_addr = nodeip[sockconnected];
        nodeaddr.sin_port = nodeport[sockconnected];
        while (ret != 0)
        {
            ret = connect(clientconnfd[sockconnected], (struct sockaddr *)&nodeaddr, sizeof(nodeaddr));
        }
        printf("connect to %x:%d\n", nodeip[sockconnected], ntohs(nodeport[sockconnected]));
        sendt.connfd = clientconnfd[sockconnected];
        sendt.no = sockconnected + 1;
        settemp.myid = sendt.no;
        *(struct SETMSG *)sendt.msg = settemp;
        //printf("sent settemp msg to node: %d\n", sendt.no);
        //新しいスレッドでsetting msgの送信、接続確立
        pthread_create(&nodethread[sockconnected], NULL, setNode, (void *)&sendt);
        pthread_join(nodethread[sockconnected], 0);
        sockconnected++;
    }
    memset(setmsg, 0, SETMSGLEN * sizeof(char));
    usleep(10000);
    printf("======start calculation======\n");
    //時間計測開始
    gettimeofday(&tv_begin, NULL);
    //結果受け取るためのサーバー

    struct SETNODE t = {0};
    //t.connfd = serverfd;
    pthread_create(&servthread, NULL, serv, (void *)&t);
    pthread_detach(servthread);

    { //message dat の読み込み
        FILE *fp;
        char ffullname[32] = {"\0"};
        char *fname1 = "messageset2010/msg";
        char *fname2 = ".dat";
        //最大ファイル数
        for (int no = 0; no < FILEMAX; no++)
        {
            memset(calmsg, 0, MAXLINE);
            sprintf(ffullname, "%s%d%s", fname1, no, fname2);
            //printf("Reading: %s\n", ffullname);
            int nodeid[10]; //node max 8
            if ((fp = fopen(ffullname, "rb")) != NULL)
            {
                //message dat の読み込み
                int count = 0;
                int ret = 0;
                do
                {
                    ret = fread(&nodeid[count], 4, 1, fp);
                    count++;
                    //printf("%x\t", nodeid[count]);
                } while (nodeid[count - 1] != 0);
                //printf("\n");
                /***nodeid転換***/
                count = count - 1;
                //nodeid[count]=0;
                //nodeid[count-1]=最後のID;

                for (int i = 0; i < count; i++)
                {
                    switch (nodeid[i])
                    {
                    case 0xc3070000:
                        nodeid[i] = (short int)1;
                        break;
                    case 0xa5090000:
                        nodeid[i] = (short int)2;
                        break;
                    case 0xf90b0000:
                        nodeid[i] = (short int)3;
                        break;
                    case 0xd3110000:
                        nodeid[i] = (short int)4;
                        break;
                    case 0x3a170000:
                        nodeid[i] = (short int)5;
                        break;
                    case 0xc4170000:
                        nodeid[i] = (short int)6;
                        break;
                    case 0x641c0000:
                        nodeid[i] = (short int)7;
                        break;
                    case 0xb5220000:
                        nodeid[i] = (short int)8;
                        break;
                    case 0x4b250000:
                        nodeid[i] = (short int)9;
                        break;
                    case 0x5b270000:
                        nodeid[i] = (short int)10;
                        break;
                    default:
                        printf("? ");
                        break;
                    }
                }
                /*** 計算用メッセージ作成***/
                struct CALMSG caltemp = {0};
                caltemp.flag = calflag;
                caltemp.fileno = no;
                caltemp.result = no;
                caltemp.count = count;
                //printf("no:", no);
                for (int i = 0; i < count; i++)
                {
                    //printf("%d, ", nodeid[i]);
                }
                //puts("");
                {
                    for (int i = 0; i < count; i++)
                    {
                        caltemp.ip[i] = nodeip[nodeid[i] - 1];
                        caltemp.port[i] = nodeport[nodeid[i] - 1];
                        //printf("%d, %d,%d\n", caltemp.ip[i], caltemp.port[i], nodeid[i] - 1);
                    }
                    //caltemp.ip[count] = servip;
                    //caltemp.port[count] = servport;
                }
                *(struct CALMSG *)calmsg = caltemp;
                //puts("caltemp:");

                // printCALMSG(&caltemp);
                //１番目の中継ノードへ発信(clientとして)
                ret = -1;
                while (ret <= 0)
                {
                    ret = writen(clientconnfd[nodeid[0] - 1], calmsg, CALMSGLEN);
                    // printf("ret=%d\n", ret);
                    //perror("send from base");
                }
                fclose(fp);
                usleep(USLEEP);
                //printf("send %hu to %hu(%d)\n", no, nodeid[0], ret);
            }
            else
            {
                printf("%s file not opened!\n", ffullname);
                //return -1;
            }
        }

    } //すべて終わったら(結果受け取ったら)、ノードへshutdownmsg//
    //pthread_join(servthread, NULL);
    pthread_mutex_destroy(&mutex);
    printf("======waiting for join======\n");
    while (resultcount < FILEMAX)
    {
        //i = 100;
        printf("%d\n", resultcount);
        usleep(100000);
    }
    printf("======recv over======\n");

    int temp = 0;
    for (; temp < FILEMAX - FILEMAX % 4; temp += 4)
    {
        printf("%d: %.2f\t", temp, results[temp]);
        printf("%d: %.2f\t", temp + 1, results[temp + 1]);
        printf("%d: %.2f\t", temp + 2, results[temp + 2]);
        printf("%d: %.2f\n", temp + 3, results[temp + 3]);
    }
    for (; temp < FILEMAX; temp++)
    {
        printf("%d: %.2f\t", temp, results[temp]);
    }
    printf("\n");

    struct CALMSG shutdowntemp = {0};
    shutdowntemp.flag = shutdownflag;
    *(struct CALMSG *)calmsg = shutdowntemp;
    for (int i = 0; i < 10; i++)
    {
        ret = -1;
        while (ret <= 0)
        {
            ret = writen(clientconnfd[i], calmsg, CALMSGLEN);
        }
    }

    for (int i = 0; i < 10; i++)
    {
        close(clientconnfd[i]);
    }

    /***　時間計測***/
    gettimeofday(&tv_end, NULL);
    printf("time: %f[s]\n", (tv_end.tv_sec - tv_begin.tv_sec) + (float)(tv_end.tv_usec - tv_begin.tv_usec) / 1000000);
    return 0;
}