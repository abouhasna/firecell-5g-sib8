#ifndef COLLECTOR_REST_MANAGER_H_
#define COLLECTOR_REST_MANAGER_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "common/utils/LOG/log.h"
#include "GNB_APP/gnb_config.h"

typedef struct collector_rest_listener_args {
    uint32_t* timeInterval;
    bool* firstTimeCopyFlag;
    bool* intervalChangedFlag;
    uint16_t port;
    char* ip;
    pthread_t collector_listener_thread;
} collector_rest_listener_args_t;


void *nr_collector_rest_listener(void *arg);

void handle_request(char* request, char* response, ...);

#endif