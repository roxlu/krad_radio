#include "krad_ipc_server.h"


krad_ipc_server_t *krad_ipc_server_init (char *sysname) {

	krad_ipc_server_t *krad_ipc_server = calloc (1, sizeof (krad_ipc_server_t));
	int i;

	if (krad_ipc_server == NULL) {
		return NULL;
	}
	
	krad_ipc_server->shutdown = KRAD_IPC_STARTING;
	
	if ((krad_ipc_server->clients = calloc (KRAD_IPC_SERVER_MAX_CLIENTS, sizeof (krad_ipc_server_client_t))) == NULL) {
		krad_ipc_server_destroy (krad_ipc_server);
	}
	
	for(i = 0; i < KRAD_IPC_SERVER_MAX_CLIENTS; i++) {
		krad_ipc_server->clients[i].krad_ipc_server = krad_ipc_server;
	}
	
	uname (&krad_ipc_server->unixname);
	if (strncmp(krad_ipc_server->unixname.sysname, "Linux", 5) == 0) {
		krad_ipc_server->on_linux = 1;
	}
		
	krad_ipc_server->sd = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

	if (krad_ipc_server->sd == -1) {
		printke ("Krad IPC Server: Socket failed.\n");
		krad_ipc_server_destroy (krad_ipc_server);
		return NULL;
	}

	krad_ipc_server->saddr.sun_family = AF_UNIX;

	if (krad_ipc_server->on_linux) {
		snprintf (krad_ipc_server->saddr.sun_path, sizeof (krad_ipc_server->saddr.sun_path), "@krad_radio_%s_ipc", sysname);	
		krad_ipc_server->saddr.sun_path[0] = '\0';
		if (connect (krad_ipc_server->sd, (struct sockaddr *) &krad_ipc_server->saddr, sizeof (krad_ipc_server->saddr)) != -1) {
			/* active socket already exists! */
			printke ("Krad IPC Server: Krad IPC path in use. (linux abstract)\n");
			krad_ipc_server_destroy (krad_ipc_server);
			return NULL;
		}
	} else {
		snprintf (krad_ipc_server->saddr.sun_path, sizeof (krad_ipc_server->saddr.sun_path), "%s/krad_radio_%s_ipc", getenv ("HOME"), sysname);	
		if (access (krad_ipc_server->saddr.sun_path, F_OK) == 0) {
			if (connect (krad_ipc_server->sd, (struct sockaddr *) &krad_ipc_server->saddr, sizeof (krad_ipc_server->saddr)) != -1) {
				/* active socket already exists! */
				printke ("Krad IPC Server: Krad IPC path in use.\n");
				krad_ipc_server_destroy (krad_ipc_server);
				return NULL;
			}
			/* remove stale socket */
			unlink (krad_ipc_server->saddr.sun_path);
		}
	}
	
	if (bind (krad_ipc_server->sd, (struct sockaddr *) &krad_ipc_server->saddr, sizeof (krad_ipc_server->saddr)) == -1) {
		printke ("Krad IPC Server: Can't bind.\n");
		krad_ipc_server_destroy (krad_ipc_server);
		return NULL;
	}

	listen (krad_ipc_server->sd, SOMAXCONN);

	krad_ipc_server->flags = fcntl (krad_ipc_server->sd, F_GETFL, 0);

	if (krad_ipc_server->flags == -1) {
		krad_ipc_server_destroy (krad_ipc_server);
		return NULL;
	}
/*
	krad_ipc_server->flags |= O_NONBLOCK;

	krad_ipc_server->flags = fcntl (krad_ipc_server->sd, F_SETFL, krad_ipc_server->flags);
	if (krad_ipc_server->flags == -1) {
		krad_ipc_server_destroy (krad_ipc_server);
		return NULL;
	}
*/	
	return krad_ipc_server;

}

int krad_ipc_server_tcp_socket_create (int port) {

	char port_string[6];
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s;
	int sfd = 0;

	snprintf (port_string, 6, "%d", port);

	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE;     /* All interfaces */

	s = getaddrinfo (NULL, port_string, &hints, &result);
	if (s != 0) {
		printke ("getaddrinfo: %s\n", gai_strerror (s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		
		sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		
		if (sfd == -1) {
			continue;
		}

		s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
		
		if (s == 0) {
			/* We managed to bind successfully! */
			break;
		}

		close (sfd);
	}

	if (rp == NULL) {
		printke ("Could not bind %d\n", port);
		return -1;
	}

	freeaddrinfo (result);

	return sfd;
}

void krad_ipc_server_disable_remote (krad_ipc_server_t *krad_ipc_server) {

	//FIXME needs to loop thru clients and disconnect remote ones

	if (krad_ipc_server->tcp_sd != 0) {
		close (krad_ipc_server->tcp_sd);
		krad_ipc_server->tcp_port = 0;
		krad_ipc_server->tcp_sd = 0;
	}
}

int krad_ipc_server_enable_remote (krad_ipc_server_t *krad_ipc_server, int port) {

	if (krad_ipc_server->tcp_sd != 0) {
		krad_ipc_server_disable_remote (krad_ipc_server);
	}
	
	krad_ipc_server->tcp_port = port;

	krad_ipc_server->tcp_sd = krad_ipc_server_tcp_socket_create (krad_ipc_server->tcp_port);

	if (krad_ipc_server->tcp_sd != 0) {
		listen (krad_ipc_server->tcp_sd, SOMAXCONN);
		krad_ipc_server_update_pollfds (krad_ipc_server);
	} else {
		krad_ipc_server->tcp_port = 0;
	}

	return 0;

}


krad_ipc_server_client_t *krad_ipc_server_accept_client (krad_ipc_server_t *krad_ipc_server, int sd) {

	krad_ipc_server_client_t *client = NULL;
	
	int i;
	struct sockaddr_un sin;
	socklen_t sin_len;
	int flags;
		
	while (client == NULL) {
		for(i = 0; i < KRAD_IPC_SERVER_MAX_CLIENTS; i++) {
			if (krad_ipc_server->clients[i].active == 0) {
				client = &krad_ipc_server->clients[i];
				break;
			}
		}
		if (client == NULL) {
			//printk ("Krad IPC Server: Overloaded with clients!\n");
			return NULL;
		}
	}

	sin_len = sizeof (sin);
	client->sd = accept (sd, (struct sockaddr *)&sin, &sin_len);

	if (client->sd >= 0) {

		flags = fcntl (client->sd, F_GETFL, 0);

		if (flags == -1) {
			close (client->sd);
			return NULL;
		}
/*
		flags |= O_NONBLOCK;

		flags = fcntl (client->sd, F_SETFL, flags);
		if (flags == -1) {
			close (client->sd);
			return NULL;
		}
*/
		client->active = 1;
		client->krad_ebml = krad_ebml_open_buffer (KRAD_EBML_IO_READONLY);
		pthread_mutex_init (&client->client_lock, NULL);
		krad_ipc_server_update_pollfds (client->krad_ipc_server);
		//printk ("Krad IPC Server: Client accepted!");	
		return client;
	}

	return NULL;
}

void krad_ipc_disconnect_client (krad_ipc_server_client_t *client) {

	pthread_mutex_lock (&client->client_lock);
	close (client->sd);
	
	if (client->krad_ebml != NULL) {
		krad_ebml_destroy (client->krad_ebml);
		client->krad_ebml = NULL;
	}
	
	if (client->krad_ebml2 != NULL) {
		krad_ebml_destroy (client->krad_ebml2);
		client->krad_ebml2 = NULL;
	}
	client->input_buffer_pos = 0;
	client->output_buffer_pos = 0;
	client->confirmed = 0;
	memset (client->input_buffer, 0, sizeof(client->input_buffer));
	memset (client->output_buffer, 0, sizeof(client->output_buffer));
	client->active = 0;
	pthread_mutex_unlock (&client->client_lock);
	pthread_mutex_destroy (&client->client_lock);
	krad_ipc_server_update_pollfds (client->krad_ipc_server);
	//printk ("Krad IPC Server: Client Disconnected");

}

/*
void krad_ipc_server_client_broadcast (krad_ipc_server_t *krad_ipc_server, char *data, int size, int broadcast_level) {

	krad_ipc_server_client_broadcast_skip(krad_ipc_server, data, size, broadcast_level, NULL);

}

void krad_ipc_server_client_broadcast_skip (krad_ipc_server_t *krad_ipc_server, char *data, int size, int broadcast_level, krad_ipc_server_client_t *client) {

	pthread_rwlock_wrlock (&krad_ipc_server->send_lock);								
	//printk ("Krad IPC Server: broadcasting start\n");

	for(krad_ipc_server->c = 0; krad_ipc_server->c < KRAD_IPC_SERVER_MAX_CLIENTS; krad_ipc_server->c++) {
		if ((krad_ipc_server->client[krad_ipc_server->c].active == 1) && (krad_ipc_server->client[krad_ipc_server->c].broadcast_level >= broadcast_level) && (&krad_ipc_server->client[krad_ipc_server->c] != client)) {
			send (krad_ipc_server->client[krad_ipc_server->c].sd, data, size, 0);
		}
	}
	
	//printk ("Krad IPC Server: broadcasting end\n");
	pthread_rwlock_unlock (&krad_ipc_server->send_lock);

	//printk ("Krad IPC Server: broadcasting: %s\n", data);

}


void krad_ipc_server_set_client_broadcasts(krad_ipc_server_t *krad_ipc_server, void *client_pointer, int broadcast_level) {

	// locking here is just becasue we are cheaply using that same c counter
	pthread_rwlock_wrlock (&krad_ipc_server->send_lock);								

	for (krad_ipc_server->c = 0; krad_ipc_server->c < KRAD_IPC_SERVER_MAX_CLIENTS; krad_ipc_server->c++) {
		if ((krad_ipc_server->client[krad_ipc_server->c].active == 1) && (krad_ipc_server->client[krad_ipc_server->c].client_pointer == client_pointer)) {

			//reset old broadcast level
			if (krad_ipc_server->client[krad_ipc_server->c].broadcast_level > 0) {
				krad_ipc_server->broadcast_clients_level[krad_ipc_server->client[krad_ipc_server->c].broadcast_level]--;
				krad_ipc_server->broadcast_clients--;
			}

			// set new
			krad_ipc_server->client[krad_ipc_server->c].broadcast_level = broadcast_level;
		}
	}
	krad_ipc_server->broadcast_clients++;
	krad_ipc_server->broadcast_clients_level[broadcast_level]++;
	
	pthread_rwlock_unlock (&krad_ipc_server->send_lock);								

}


void krad_ipc_server_client_send (void *client, char *data) {

	krad_ipc_client_t *kclient = (krad_ipc_client_t *)client;

	send (kclient->sd, data, strlen(data), 0);

}
*/

void krad_ipc_server_update_pollfds (krad_ipc_server_t *krad_ipc_server) {

	int c;
	int s;

	s = 0;

	krad_ipc_server->sockets[s].fd = krad_ipc_server->sd;
	krad_ipc_server->sockets[s].events = POLLIN;

	s++;
	
	if (krad_ipc_server->tcp_sd != 0) {
		krad_ipc_server->sockets[s].fd = krad_ipc_server->tcp_sd;
		krad_ipc_server->sockets[s].events = POLLIN;
		s++;
	}

	for (c = 0; c < KRAD_IPC_SERVER_MAX_CLIENTS; c++) {
		if (krad_ipc_server->clients[c].active == 1) {
			krad_ipc_server->sockets[s].fd = krad_ipc_server->clients[c].sd;
			krad_ipc_server->sockets[s].events |= POLLIN;			
			krad_ipc_server->sockets_clients[s] = &krad_ipc_server->clients[c];
			s++;
		}
	}
	
	krad_ipc_server->socket_count = s;

	//printk ("Krad IPC Server: sockets rejiggerd!\n");	

}


int krad_ipc_server_read_command (krad_ipc_server_t *krad_ipc_server, uint32_t *ebml_id_ptr, uint64_t *ebml_data_size_ptr) {

	return krad_ebml_read_element (krad_ipc_server->current_client->krad_ebml, ebml_id_ptr, ebml_data_size_ptr);
}

uint64_t krad_ipc_server_read_number (krad_ipc_server_t *krad_ipc_server, uint64_t data_size) {

	return krad_ebml_read_number (krad_ipc_server->current_client->krad_ebml, data_size);
}

void krad_ipc_server_read_tag ( krad_ipc_server_t *krad_ipc_server, char **tag_item, char **tag_name, char **tag_value ) {

	uint32_t ebml_id;
	uint64_t ebml_data_size;

	krad_ebml_read_element (krad_ipc_server->current_client->krad_ebml, &ebml_id, &ebml_data_size);
	
	if (ebml_id != EBML_ID_KRAD_RADIO_TAG) {
		printke ("hrm wtf\n");
	} else {
		//printf("tag size %zu\n", ebml_data_size);
	}
	
	krad_ebml_read_element (krad_ipc_server->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

	if (ebml_id != EBML_ID_KRAD_RADIO_TAG_ITEM) {
		printke ("hrm wtf2\n");
	} else {
		//printf("tag name size %zu\n", ebml_data_size);
	}

	krad_ebml_read_string (krad_ipc_server->current_client->krad_ebml, *tag_item, ebml_data_size);	
	
	krad_ebml_read_element (krad_ipc_server->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

	if (ebml_id != EBML_ID_KRAD_RADIO_TAG_NAME) {
		printke ("hrm wtf2\n");
	} else {
		//printf("tag name size %zu\n", ebml_data_size);
	}

	krad_ebml_read_string (krad_ipc_server->current_client->krad_ebml, *tag_name, ebml_data_size);
	
	krad_ebml_read_element (krad_ipc_server->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

	if (ebml_id != EBML_ID_KRAD_RADIO_TAG_VALUE) {
		printke ("hrm wtf3\n");
	} else {
		//printf("tag value size %zu\n", ebml_data_size);
	}

	krad_ebml_read_string (krad_ipc_server->current_client->krad_ebml, *tag_value, ebml_data_size);
	
}

void krad_ipc_server_response_start ( krad_ipc_server_t *krad_ipc_server, uint32_t ebml_id, uint64_t *response) {

	krad_ebml_start_element (krad_ipc_server->current_client->krad_ebml2, ebml_id, response);

}

void krad_ipc_server_response_finish ( krad_ipc_server_t *krad_ipc_server, uint64_t response) {

	krad_ebml_finish_element (krad_ipc_server->current_client->krad_ebml2, response);

}

void krad_ipc_server_response_list_start ( krad_ipc_server_t *krad_ipc_server, uint32_t ebml_id, uint64_t *list) {

	krad_ebml_start_element (krad_ipc_server->current_client->krad_ebml2, ebml_id, list);

}

void krad_ipc_server_response_add_tag ( krad_ipc_server_t *krad_ipc_server, char *tag_item, char *tag_name, char *tag_value) {

	uint64_t tag;

	krad_ebml_start_element (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_RADIO_TAG, &tag);	
	krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_RADIO_TAG_ITEM, tag_item);
	krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_RADIO_TAG_NAME, tag_name);
	krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_RADIO_TAG_VALUE, tag_value);	
	//krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_RADIO_TAG_SOURCE, "");
	krad_ebml_finish_element (krad_ipc_server->current_client->krad_ebml2, tag);

}

void krad_ipc_server_response_add_portgroup ( krad_ipc_server_t *krad_ipc_server, char *name, int channels,
											  int io_type, float volume, char *mixbus, char *crossfade_name, float crossfade_value ) {

	uint64_t portgroup;

	krad_ebml_start_element (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP, &portgroup);	

	krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_NAME, name);
	krad_ebml_write_int8 (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_CHANNELS, channels);
	if (io_type == 0) {
		krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_TYPE, "Jack");
	} else {
		krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_TYPE, "Internal");
	}
	krad_ebml_write_float (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_VOLUME, volume);	
	krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_MIXBUS, mixbus);			

	krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_CROSSFADE_NAME, crossfade_name);
	krad_ebml_write_float (krad_ipc_server->current_client->krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_CROSSFADE, crossfade_value);	

	krad_ebml_finish_element (krad_ipc_server->current_client->krad_ebml2, portgroup);

}

void krad_ipc_server_broadcast_portgroup_created ( krad_ipc_server_t *krad_ipc_server, char *name, int channels,
											  	   int io_type, float volume, char *mixbus ) {

	int c;
	uint64_t portgroup;
	uint64_t element;
	uint64_t subelement;

	for (c = 0; c < KRAD_IPC_SERVER_MAX_CLIENTS; c++) {
		if ((krad_ipc_server->clients[c].confirmed == 1) && (krad_ipc_server->current_client != &krad_ipc_server->clients[c])) {

			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_MSG, &element);	
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_CREATED, &subelement);

			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP, &portgroup);	

			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_NAME, name);
			krad_ebml_write_int8 (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_CHANNELS, channels);
			if (io_type == 0) {
				krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_TYPE, "Jack");
			} else {
				krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_TYPE, "Internal");
			}
			krad_ebml_write_float (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_VOLUME, volume);	
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_MIXBUS, mixbus);			

			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_CROSSFADE_NAME, "");
			krad_ebml_write_float (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_CROSSFADE, 0.0);	

			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, portgroup);
			
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, subelement);
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, element);
			krad_ebml_write_sync (krad_ipc_server->clients[c].krad_ebml2);		
			
		}
	}
}

void krad_ipc_server_response_list_finish ( krad_ipc_server_t *krad_ipc_server, uint64_t list) {

	krad_ebml_finish_element (krad_ipc_server->current_client->krad_ebml2, list);


}

void krad_ipc_server_respond_number ( krad_ipc_server_t *krad_ipc_server, uint32_t ebml_id, uint64_t number) {

	krad_ebml_write_int64 (krad_ipc_server->current_client->krad_ebml2, ebml_id, number);

}

void krad_ipc_server_respond_string ( krad_ipc_server_t *krad_ipc_server, uint32_t ebml_id, char *string) {

	krad_ebml_write_string (krad_ipc_server->current_client->krad_ebml2, ebml_id, string);

}


void krad_ipc_server_simple_broadcast ( krad_ipc_server_t *krad_ipc_server, uint32_t ebml_id, uint32_t ebml_subid, uint32_t ebml_subid2, char *string) {

	int c;

	uint64_t element;
	uint64_t subelement;

	element = 0;
	subelement = 0;

	for (c = 0; c < KRAD_IPC_SERVER_MAX_CLIENTS; c++) {
		if ((krad_ipc_server->clients[c].confirmed == 1) && (krad_ipc_server->current_client != &krad_ipc_server->clients[c])) {
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, ebml_id, &element);	
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, ebml_subid, &subelement);	
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, ebml_subid2, string);
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, subelement);
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, element);
			krad_ebml_write_sync (krad_ipc_server->clients[c].krad_ebml2);
		}
	}
}

void krad_ipc_server_mixer_broadcast2 ( krad_ipc_server_t *krad_ipc_server, uint32_t ebml_id, uint32_t ebml_subid, char *portname, uint32_t ebml_subid2, char *string) {

	int c;

	uint64_t element;
	uint64_t subelement;

	element = 0;
	subelement = 0;

	for (c = 0; c < KRAD_IPC_SERVER_MAX_CLIENTS; c++) {
		if ((krad_ipc_server->clients[c].confirmed == 1) && (krad_ipc_server->current_client != &krad_ipc_server->clients[c])) {
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, ebml_id, &element);	
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, ebml_subid, &subelement);	
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_NAME, portname);
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, ebml_subid2, string);
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, subelement);
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, element);
			krad_ebml_write_sync (krad_ipc_server->clients[c].krad_ebml2);
		}
	}
}

void krad_ipc_server_mixer_broadcast ( krad_ipc_server_t *krad_ipc_server, uint32_t ebml_id, uint32_t ebml_subid, char *portname, char *controlname, float floatval) {

	int c;

	uint64_t element;
	uint64_t subelement;

	element = 0;
	subelement = 0;

	for (c = 0; c < KRAD_IPC_SERVER_MAX_CLIENTS; c++) {
		if ((krad_ipc_server->clients[c].confirmed == 1) && (krad_ipc_server->current_client != &krad_ipc_server->clients[c])) {
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, ebml_id, &element);	
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, ebml_subid, &subelement);	
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_PORTGROUP_NAME, portname);
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_CONTROL_NAME, controlname);
			krad_ebml_write_float (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_MIXER_CONTROL_VALUE, floatval);
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, subelement);
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, element);
			krad_ebml_write_sync (krad_ipc_server->clients[c].krad_ebml2);
		}
	}
}

void krad_ipc_server_broadcast_tag ( krad_ipc_server_t *krad_ipc_server, char *item, char *name, char *value) {

	int c;

	uint64_t element;
	uint64_t subelement;

	element = 0;
	subelement = 0;

	for (c = 0; c < KRAD_IPC_SERVER_MAX_CLIENTS; c++) {
		//if ((krad_ipc_server->clients[c].confirmed == 1) && (krad_ipc_server->current_client != &krad_ipc_server->clients[c])) {
		if (krad_ipc_server->clients[c].confirmed == 1) {		
			pthread_mutex_lock (&krad_ipc_server->clients[c].client_lock);
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_RADIO_MSG, &element);	
			krad_ebml_start_element (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_RADIO_TAG, &subelement);	
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_RADIO_TAG_ITEM, item);
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_RADIO_TAG_NAME, name);
			krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_RADIO_TAG_VALUE, value);
			//krad_ebml_write_string (krad_ipc_server->clients[c].krad_ebml2, EBML_ID_KRAD_RADIO_TAG_SOURCE, "");
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, subelement);
			krad_ebml_finish_element (krad_ipc_server->clients[c].krad_ebml2, element);
			krad_ebml_write_sync (krad_ipc_server->clients[c].krad_ebml2);
			pthread_mutex_unlock (&krad_ipc_server->clients[c].client_lock);
		}
	}
}


void *krad_ipc_server_run_thread (void *arg) {

	krad_ipc_server_t *krad_ipc_server = (krad_ipc_server_t *)arg;
	krad_ipc_server_client_t *client;
	int ret, s;
	
	krad_ipc_server->shutdown = KRAD_IPC_RUNNING;
	
	krad_ipc_server_update_pollfds (krad_ipc_server);

	while (!krad_ipc_server->shutdown) {

		ret = poll (krad_ipc_server->sockets, krad_ipc_server->socket_count, KRAD_IPC_SERVER_TIMEOUT_MS);

		if (ret > 0) {
		
			if (krad_ipc_server->shutdown) {
				break;
			}
	
			s = 0;
	
			if (krad_ipc_server->sockets[s].revents & POLLIN) {
				krad_ipc_server_accept_client (krad_ipc_server, krad_ipc_server->sd);
				ret--;
			}
			
			s++;
			
			if ((krad_ipc_server->tcp_sd != 0) && (krad_ipc_server->sockets[s].revents & POLLIN)) {
				krad_ipc_server_accept_client (krad_ipc_server, krad_ipc_server->tcp_sd);
				ret--;
			}			

			if (krad_ipc_server->tcp_sd != 0) {
				s++;
			}
	
			for (; ret > 0; s++) {

				if (krad_ipc_server->sockets[s].revents) {
					
					ret--;
				
					client = krad_ipc_server->sockets_clients[s];
				
					if (krad_ipc_server->sockets[s].revents & POLLIN) {
	
						client->input_buffer_pos += recv(krad_ipc_server->sockets[s].fd, client->input_buffer + client->input_buffer_pos, (sizeof (client->input_buffer) - client->input_buffer_pos), 0);
					
						//printk ("Krad IPC Server: Got %d bytes\n", client->input_buffer_pos);
					
						if (client->input_buffer_pos == 0) {
							//printk ("Krad IPC Server: Client EOF\n");
							krad_ipc_disconnect_client (client);
							continue;
						}

						if (client->input_buffer_pos == -1) {
							printke ("Krad IPC Server: Client Socket Error");
							krad_ipc_disconnect_client (client);
							continue;
						}
					
						// big enough to read element id and data size
						if ((client->input_buffer_pos > 7) && (client->confirmed == 0)) {
							krad_ebml_io_buffer_push(&client->krad_ebml->io_adapter, client->input_buffer, client->input_buffer_pos);
							client->input_buffer_pos = 0;
							
							krad_ebml_read_ebml_header (client->krad_ebml, client->krad_ebml->header);
							krad_ebml_check_ebml_header (client->krad_ebml->header);
							//krad_ebml_print_ebml_header (client->krad_ebml->header);
							
							if (krad_ebml_check_doctype_header (client->krad_ebml->header, KRAD_IPC_CLIENT_DOCTYPE, KRAD_IPC_DOCTYPE_VERSION, KRAD_IPC_DOCTYPE_READ_VERSION)) {
								//printk ("Matched %s Version: %d Read Version: %d\n", KRAD_IPC_CLIENT_DOCTYPE, KRAD_IPC_DOCTYPE_VERSION, KRAD_IPC_DOCTYPE_READ_VERSION);
								client->confirmed = 1;
							} else {
								printke ("Did Not Match %s Version: %d Read Version: %d\n", KRAD_IPC_CLIENT_DOCTYPE, KRAD_IPC_DOCTYPE_VERSION, KRAD_IPC_DOCTYPE_READ_VERSION);
								krad_ipc_disconnect_client (client);
								continue;
							}
							
							
							client->krad_ebml2 = krad_ebml_open_active_socket (client->sd, KRAD_EBML_IO_READWRITE);

							krad_ebml_header_advanced (client->krad_ebml2, KRAD_IPC_SERVER_DOCTYPE, KRAD_IPC_DOCTYPE_VERSION, KRAD_IPC_DOCTYPE_READ_VERSION);
							krad_ebml_write_sync (client->krad_ebml2);
							
						} else {
							if ((client->input_buffer_pos > 3) && (client->confirmed == 1)) {
								krad_ebml_io_buffer_push(&client->krad_ebml->io_adapter, client->input_buffer, client->input_buffer_pos);
								client->input_buffer_pos = 0;
							}
						
							while (krad_ebml_io_buffer_read_space (&client->krad_ebml->io_adapter)) {
								client->krad_ipc_server->current_client = client; /* single thread has a few perks */
								pthread_mutex_lock (&client->client_lock);
								//resp = client->krad_ipc_server->handler (client->output_buffer, &client->command_response_len, client->krad_ipc_server->pointer);
								client->krad_ipc_server->handler (client->output_buffer, &client->command_response_len, client->krad_ipc_server->pointer);
								//printk ("Krad IPC Server: CMD Response %d %d bytes\n", resp, client->command_response_len);
								krad_ebml_write_sync (krad_ipc_server->current_client->krad_ebml2);
								pthread_mutex_unlock (&client->client_lock);
							}
						
						}
						//printk ("Krad IPC Server: got %d bytes\n", client->bytes);
	/*	
						while (strchr(client->recbuffer, '|') != NULL) {
						
							//printk ("Krad IPC Server: found delmitmer\n");
						
							client->msglen = strcspn(client->recbuffer, "|");
							strncpy(client->buffer, client->recbuffer, client->msglen);
							if (client->msglen == (client->bytes - 1)) {
								//printk ("Krad IPC Server: one msg buf, nuked\n");
								client->bytes = 0;
								client->recbuffer[client->bytes] = '\0';
							} else {
								//printk ("Krad IPC Server: multi .. memmoved\n");
								memmove(client->recbuffer, client->recbuffer + client->msglen + 1, client->bytes - (client->msglen + 1));
								client->bytes = client->bytes - (client->msglen + 1);
							}

	
							client->buffer[client->msglen] = '\0';
	
							if (client->krad_ipc_server->handler != NULL) {
								//client->broadcast = client->krad_ipc_server->handler(client->buffer, client->client_pointer);
							}
		
							// we should be adding 1 to this for the null value
							// but we add it in in the client libs, so whatever
							client->rbytes = (strlen(client->buffer));
							printk ("Krad IPC Server: broadcast was %d bytes returned is %d\n", client->broadcast, client->rbytes);
							// returned bytes
							if (client->rbytes > 0) {

								// 0 = no broadcast, 1 = broadcast to everyone but me, 2 = broadcast to everyone including back to me
								if (client->broadcast) {
				
									if (client->broadcast == 1) {
										krad_ipc_server_client_broadcast_skip(client->krad_ipc_server, client->buffer, client->rbytes, 1, client);
									} else {
										krad_ipc_server_client_broadcast(client->krad_ipc_server, client->buffer, client->rbytes, 1);
									}
									client->broadcast = 0;
								} else {
									printk ("Krad IPC Server: sending %s\n", client->buffer);

									send (client->sd, client->buffer, client->rbytes, 0);

								}
							}
						}
	*/	
					}
					
					
					
				
					if (krad_ipc_server->sockets[s].revents & POLLOUT) {
						//printk ("I could write\n");
					}

					if (krad_ipc_server->sockets[s].revents & POLLHUP) {
						//printk ("Krad IPC Server: POLLHUP\n");
						krad_ipc_disconnect_client (client);
						continue;
					}

					if (krad_ipc_server->sockets[s].revents & POLLERR) {
						printke ("Krad IPC Server: POLLERR\n");
						krad_ipc_disconnect_client (client);
						continue;
					}

					if (krad_ipc_server->sockets[s].revents & POLLNVAL) {
						printke ("Krad IPC Server: POLLNVAL\n");
						krad_ipc_disconnect_client (client);
						continue;
					}
					
					
					
					
				}
			}
		}
	}

	krad_ipc_server->shutdown = KRAD_IPC_SHUTINGDOWN;

	return NULL;

}


void krad_ipc_server_destroy (krad_ipc_server_t *krad_ipc_server) {

	int c;
	int patience;
	
	patience = KRAD_IPC_SERVER_TIMEOUT_US * 3;
	
	if (krad_ipc_server->shutdown == KRAD_IPC_RUNNING) {
		krad_ipc_server->shutdown = KRAD_IPC_DO_SHUTDOWN;
	
		while ((krad_ipc_server->shutdown != KRAD_IPC_SHUTINGDOWN) && (patience > 0)) {
			usleep (KRAD_IPC_SERVER_TIMEOUT_US / 4);
			patience -= KRAD_IPC_SERVER_TIMEOUT_US / 4;
		}
	}
	
	if (krad_ipc_server->tcp_sd != 0) {
		krad_ipc_server_disable_remote (krad_ipc_server);
	}	

	if (krad_ipc_server->sd != 0) {
		close (krad_ipc_server->sd);
		if (!krad_ipc_server->on_linux) {
			unlink (krad_ipc_server->saddr.sun_path);
		}
	}

	for (c = 0; c < KRAD_IPC_SERVER_MAX_CLIENTS; c++) {
		if (krad_ipc_server->clients[c].active == 1) {
			krad_ipc_disconnect_client (&krad_ipc_server->clients[c]);
		}
	}
	
	
	free (krad_ipc_server->clients);
	free (krad_ipc_server);
	
}

void krad_ipc_server_run (krad_ipc_server_t *krad_ipc_server) {
	pthread_create (&krad_ipc_server->server_thread, NULL, krad_ipc_server_run_thread, (void *)krad_ipc_server);
	pthread_detach (krad_ipc_server->server_thread);
}

krad_ipc_server_t *krad_ipc_server_create (char *sysname, int handler (void *, int *, void *), void *pointer) {


	krad_ipc_server_t *krad_ipc_server;
	
	krad_ipc_server = krad_ipc_server_init (sysname);

	if (krad_ipc_server == NULL) {
		return NULL;
	}
	
	krad_ipc_server->handler = handler;
	krad_ipc_server->pointer = pointer;

	return krad_ipc_server;	

}

