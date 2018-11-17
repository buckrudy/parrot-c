/*
 * Author: leegoogol 
 * Email: buckgugle@gamil.com
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

struct Frame {
	unsigned char *data;
	int data_len;
};

#define FRAME_HEAD "\033[2J\033[H"

#define YELLOW "\033[33m"
#define GREEN  "\033[32m"
#define BLUE   "\033[34m"
#define NONE   "\033[0m"

int head_len = sizeof(FRAME_HEAD)-1 + sizeof(YELLOW)-1;
int tail_len = sizeof(NONE)-1;

unsigned char headers[] = "HTTP/1.1 200 OK\nServer: nginx/1.12.1 (Ubuntu)\nConnection: keep-alive\n\n";
unsigned char header_redirect[] = "HTTP/1.1 302 Moved Temporarily\nServer: nginx/1.12.1 (Ubuntu)\nLocation: https://github.com/buckrudy/parrot-c\n\n";

unsigned char *colors[] = {
	YELLOW,
	GREEN,
	BLUE,
};

struct Frame data_frames[10];

void set_signal_block(void)
{
	sigset_t set;
#if 0
	sigfillset(&set);
	sigdelset(&set, SIGINT);
#else
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);	//如果不阻塞此信号，当对端关闭后，还继续写，进程将收到此信号。
#endif
	sigprocmask(SIG_BLOCK, &set, NULL);
}

void read_all_frames(void)
{
	int i, fd;
	char name[1024];
	struct stat st;

	for (i=0; i<10; i++) {
		sprintf(name, "frames/%d.txt", i);
		fd = open(name, O_RDONLY);
		if (fd < 0) {
			printf("open error\n");
			exit(15);
		}
		fstat(fd, &st);
		data_frames[i].data_len = st.st_size + head_len + tail_len;
		data_frames[i].data = malloc(data_frames[i].data_len);
		memcpy(data_frames[i].data, FRAME_HEAD, sizeof(FRAME_HEAD)-1);
		memcpy(data_frames[i].data + data_frames[i].data_len - tail_len, NONE, tail_len);
		read(fd, data_frames[i].data + head_len, st.st_size);
		close(fd);
	}
}

void *parrot_thread(void *arg)
{
	int client = (int)arg;
	unsigned char rbuf[1024];
	struct timespec req = {.tv_sec = 0, .tv_nsec = 70 * 1000 * 1000L};
	int rn, i = 0, color = 0;

	pthread_detach(pthread_self());

	read(client, rbuf, sizeof(rbuf));

	if (NULL == strcasestr(rbuf, "User-Agent: curl")) {	//Not used curl OR change the http header manually
		write(client, header_redirect, sizeof(header_redirect)-1);
		close(client);
		return NULL;
	}
	
	write(client, headers, sizeof(headers)-1);

	do {
		memcpy(data_frames[i].data + sizeof(FRAME_HEAD)-1, colors[color], strlen(colors[color]));
		rn = write(client, data_frames[i].data, data_frames[i].data_len);
		if (rn <= 0 && errno != EINTR) {
			printf("write error %d, [%s]\n", rn, strerror(errno));
			fflush(stdout);
			break;
		}
		nanosleep(&req, NULL);
		i++;
		color++;
		if (i==10)
			i = 0;
		if (color==3)
			color = 0;
	} while (1);

	close(client);
	return NULL;
}

int main(int argc, char **argv)
{
	int sock, client;
	struct sockaddr_in saddr;
	int port, reuse = 1;
	pthread_t tid;

	if (argc != 2) {
		printf("Usage: %s <PORT>\n\n", argv[0]);
		exit(1);
	} else {
		port = atoi(argv[1]);
		if (0 >= port) {
			port = 10800;
			printf("invalid port [%s], used default 10800\n", argv[1]);
		}
	}

	set_signal_block();

	sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock < 0) {
		printf("error\n");
		fflush(stdout);
		exit(1);
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(port);

	if (0 != bind(sock, (struct sockaddr*)&saddr, sizeof(saddr))) {
		printf("bind error\n");
		fflush(stdout);
		exit(2);
	}

	if (0 != listen(sock, 5)) {
		printf("listen error\n");
		fflush(stdout);
		exit(3);
	}

	read_all_frames();

	for (;;) {
		client = accept(sock, NULL, NULL);
		if (client != -1) {
			if (0 != pthread_create(&tid, NULL, parrot_thread, (void *)client)) {
				printf("pthread_create error\n");
				fflush(stdout);
				close(client);
			}
		}
	}
	return 0;
}
