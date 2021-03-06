#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>

#include "krad_system.h"

#define BUFSIZE 8192

typedef struct krad_http_St krad_http_t;
typedef struct krad_http_client_St krad_http_client_t;

struct krad_http_St {

	krad_http_client_t *current_client;
	krad_http_client_t *clients;
	
	int port;
	
	int listenfd;
	int socketfd;
	char *homedir;
	
	int shutdown;
	
	int websocket_port;	
	char *html;
	int html_len;
	char *js;
	int js_len;
	
	pthread_t server_thread;	
	
};

struct krad_http_client_St {

	krad_http_t *krad_http;
	pthread_t client_thread;

	char in_buffer[8192];
	char out_buffer[8192];
	char get[256];
	char filename[256];
	
	int in_buffer_pos;
	int out_buffer_pos;
	
	int sd;
	int file_fd;
	int ret;
	int wrote;

};


krad_http_client_t *create_client(krad_http_t *krad_http);
void krad_http_destroy_client(krad_http_t *krad_http, krad_http_client_t *client);
void krad_http_write_headers (krad_http_client_t *client, char *content_type);
void krad_http_404 (krad_http_client_t *client);

void *krad_http_server_run (void *arg);
krad_http_t *krad_http_server_create (int port, int websocket_port);
void krad_http_server_destroy (krad_http_t *krad_http);


