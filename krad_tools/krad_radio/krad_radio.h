#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <time.h>
#include <sys/stat.h>

#include "krad_ipc_server.h"
#include "krad_radio_ipc.h"
#include "krad_websocket.h"
#include "krad_http.h"

#include "krad_mixer.h"
#include "krad_tags.h"

typedef struct krad_radio_St krad_radio_t;

struct krad_radio_St {

	char *name;
	char *callsign;
	krad_ipc_server_t *ipc;
	krad_websocket_t *krad_websocket;
	krad_http_t *krad_http;
	krad_mixer_t *krad_mixer;


	int test_value;

	krad_tags_t *krad_tags;

};



void krad_radio_destroy(krad_radio_t *krad_radio);
krad_radio_t *krad_radio_create(char *callsign_or_config);
void krad_radio_daemonize ();
void krad_radio_run (krad_radio_t *krad_radio);
void krad_radio (char *callsign_or_config);

int krad_radio_handler ( void *output, int *output_len, void *ptr );
