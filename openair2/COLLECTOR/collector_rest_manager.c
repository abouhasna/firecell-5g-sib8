#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <time.h>

#include "collector_rest_manager.h"
#include "cu_collector.h"

#define GET_REQUEST "GET"
#define POST_REQUEST "POST"

#define GET_REQUEST_CU "GET /cu"

#define GET_REQUEST_DU "GET /du"

#define GET_CU_INFO_CELL "GET /cu/info/cell"
#define GET_CU_INFO_UE "GET /cu/info/ue"
#define GET_CU_STATUS "GET /cu/status"

#define GET_DU_STATUS "GET /du/status"

#define MAX_JSON_SIZE_CELL 1024
#define MAX_JSON_SIZE_CELL_LIST (MAX_JSON_SIZE_CELL * COLLECTOR_CELL_NUMBER)
#define MAX_JSON_SIZE_UE 1024
#define MAX_JSON_SIZE_UE_LIST (MAX_JSON_SIZE_UE * COLLECTOR_UE_NUMBER)
#define MAX_BUFFER_SIZE (MAX_JSON_SIZE_CELL + MAX_JSON_SIZE_CELL_LIST + MAX_JSON_SIZE_UE + MAX_JSON_SIZE_UE_LIST)

extern cu_collector_struct_t cu_collector_struct;

static void handle_get_request_cu(char* request, char* response);
static void handle_get_request_du(char* request, char* response);
static void handle_get_request(char* request, char* response);
static char* handle_post_request(char* request, void* args);
static void create_cell_information_response(char* json_buffer);
static void create_ue_information_response(char* json_buffer);

void *nr_collector_rest_listener(void *arg) {
  LOG_I(FC_COLLECTOR, "REST LISTENER IS CREATED!\n");

  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  char buffer[MAX_BUFFER_SIZE] = {0};
  collector_rest_listener_args_t* collector_rest_listener_args = (collector_rest_listener_args_t*)arg;
  if (!collector_rest_listener_args) {
    LOG_E(FC_COLLECTOR, "Collector HTTP Args is NULL\n");
    exit(EXIT_FAILURE);
  }

    if (!collector_rest_listener_args->port){
      LOG_E(FC_COLLECTOR, "Collector HTTP Port is NULL\n");
      exit(EXIT_FAILURE);
    }


    // Create a socket for the server
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Allow the socket to be reused immediately after closing
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Set socket options failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to a port and listen for incoming connections
    address.sin_family = AF_INET;
    LOG_I(FC_COLLECTOR,
          "HTTP listener Port %u and Host details %s \n",
          collector_rest_listener_args->port,
          collector_rest_listener_args->ip);
    address.sin_port = htons(collector_rest_listener_args->port);
    if (inet_pton(AF_INET, collector_rest_listener_args->ip, &address.sin_addr.s_addr) != 1) {
      // Handle conversion error
      fprintf(stderr, "Invalid IP address format or conversion failed\n");
      assert(0);
    }
    free(collector_rest_listener_args->ip);
    LOG_D(FC_COLLECTOR, "Converted HTTP listener Port %u and Host details %u \n", address.sin_port, address.sin_addr.s_addr);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Loop indefinitely and wait for incoming requests
    while (oai_exit == 0) {
      LOG_D(FC_COLLECTOR, "COLLECTOR REST: Waiting for incoming requests...\n");

      // Accept the incoming connection and read the request
      if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        perror("Accept failed");
        continue;
        }
        if (read(new_socket, buffer, MAX_BUFFER_SIZE) < 0) {
          perror("Read failed\n");
          exit(EXIT_FAILURE);
        }

        char response[MAX_BUFFER_SIZE] = {0};
        handle_request(buffer, response, arg);
        LOG_D(FC_COLLECTOR, "Response: %s\n", response);

        send(new_socket, response, strlen(response), 0);
        close(new_socket);
    }
    return NULL;
}

void handle_request(char* request, char* response, ...)
{
  if (strncmp(request, GET_REQUEST, strlen(GET_REQUEST)) == 0) {
    LOG_D(FC_COLLECTOR, "HTTP_LISTENER: GET Request has came to the collector!\n");
    handle_get_request(request, response);
  } else if (strncmp(request, POST_REQUEST, strlen(POST_REQUEST)) == 0) {
    LOG_D(FC_COLLECTOR, "HTTP_LISTENER: POST Request has came to the collector!\n");
    va_list rest_args;
    va_start(rest_args, response);
    void* request_arguments = va_arg(rest_args, void*);
    sprintf(response, "%s", handle_post_request(request, request_arguments));
    va_end(rest_args);
  } else {
    LOG_D(FC_COLLECTOR, "HTTP_LISTENER: Bad Request has came to the collector!\n");
    sprintf(response, "HTTP/1.1 400 Bad Request\n\n");
  }
}

static void handle_get_request(char* request, char* response)
{
  if (strncmp(request, GET_REQUEST_CU, strlen(GET_REQUEST_CU)) == 0) {
    pthread_mutex_lock(&cu_collector_struct.cu_collector_mutex);
    handle_get_request_cu(request, response);
    pthread_mutex_unlock(&cu_collector_struct.cu_collector_mutex);
  } else if (strncmp(request, GET_REQUEST_DU, strlen(GET_REQUEST_DU)) == 0) {
    handle_get_request_du(request, response);
  } else {
    strcat(response, "HTTP/1.1 400 Bad Request\n\n");
  }
}

static char* handle_post_request(char* request, void* args)
{
    char *param = strstr(request, "time_interval=");
    if (param != NULL) {
        char *value = param + strlen("time_interval=");
        int time_interval = atoi(value);

        collector_rest_listener_args_t* timeIntervalArgs = (collector_rest_listener_args_t*)args;
        // Set the time interval
        LOG_I(FC_COLLECTOR, "COLLECTOR: Received time interval: %d seconds\n", time_interval);
        *(timeIntervalArgs->timeInterval) = time_interval;
        *(timeIntervalArgs->firstTimeCopyFlag) = true;
        *(timeIntervalArgs->intervalChangedFlag) = true;
        return "HTTP/1.1 200 Time Interval Changed\n\n";
    }
    return "HTTP/1.1 400 Bad Request\n\n";
}

static void create_json_response_header(char* response)
{
    strcpy(response, "HTTP/1.1 200 OK\r\n");
    strcat(response, "Content-Type: application/json\r\n\r\n");
}
static void handle_get_request_cu(char* request, char* response)
{
  if (strncmp(request, GET_CU_INFO_CELL, strlen(GET_CU_INFO_CELL)) == 0) {
    LOG_D(FC_COLLECTOR, "HTTP_LISTENER: GET Request for CU Cell Info\n");
    create_json_response_header(response);

    char cell_list_json[MAX_JSON_SIZE_CELL_LIST] = "";
    create_cell_information_response(cell_list_json);

    strcat(response, cell_list_json);
  } else if (strncmp(request, GET_CU_INFO_UE, strlen(GET_CU_INFO_UE)) == 0) {
    LOG_D(FC_COLLECTOR, "HTTP_LISTENER: GET Request for CU UE Info\n");
    create_json_response_header(response);

    char ue_list_json[MAX_JSON_SIZE_CELL_LIST] = "";
    create_ue_information_response(ue_list_json);

    strcat(response, ue_list_json);

  } else if (strncmp(request, GET_CU_STATUS, strlen(GET_CU_STATUS)) == 0) {
    LOG_D(FC_COLLECTOR, "HTTP_LISTENER: GET Request for CU Status\n");
    strcat(response, "HTTP/1.1 200 OK\r\n");
  } else {
    LOG_E(FC_COLLECTOR, "HTTP_LISTENER: Bad Request for CU: %s\n", request);
    strcat(response, "HTTP/1.1 400 Bad Request\n");
  }

  strcat(response, "\n");
}

static void handle_get_request_du(char* request, char* response)
{
  if (strncmp(request, GET_DU_STATUS, strlen(GET_DU_STATUS)) == 0) {
    strcat(response, "HTTP/1.1 200 OK\r\n");
    return;
  }
  strcat(response, "HTTP/1.1 400 Bad Request\n\n");
}

// CELL JSON FUNCTIONS


static void create_cell_json_string(char* cell_json, cu_collector_cell_info_t* cell)
{
  time_t current_time = time(NULL);
  char timestamp[20];
  strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&current_time));
  sprintf(cell_json,
          "    {\n"
          "      \"scs\": \"%u\",\n"
          "      \"cell_id\": \"%" PRIu64
          "\",\n"
          "      \"dl_carrier_freq\": \"%" PRIu64
          "\",\n"
          "      \"downlink_bw\": \"%u\",\n"
          "      \"uplink_bw\": \"%u\",\n"
          "      \"dl_freq_band\": \"%d\",\n"
          "      \"cell_state\": \"%d\",\n"
          "      \"gnb_id\": \"%u\",\n"
          "      \"number_of_connected_ues\": \"%d\",\n"
          "      \"number_of_active_ues\": \"%d\",\n"
          "      \"tracking_area_code\": \"%u\"\n"
          "    }",
          cell->scs,
          cell->nr_cell_id,
          cell->dl_carrier_frequency_hz,
          cell->dl_bandwidth,
          cell->ul_bandwidth,
          cell->dl_freq_band,
          cell->cell_state,
          cell->gnb_id,
          cell->number_of_connected_ues,
          cell->number_of_active_ues,
          cell->tac);
}

static void create_cell_list_json_string(char* cell_list_json)
{
  strcat(cell_list_json, "  \"cell_list\": [");
  uint8_t nb_of_cells = get_cu_collector_cell_number();
  LOG_D(FC_COLLECTOR, "CU Collector: Number of Cells: %d\n", nb_of_cells);

  if (nb_of_cells == 0) {
    strcat(cell_list_json, "\n  ]");
    return;
  }

  int added_cells = 0;

  for (int i = 0; i < COLLECTOR_CELL_NUMBER; i++) {
    cu_collector_cell_info_t* cell = cu_collector_struct.cell_list[i];
    if (!cell) {
      continue;
    }

    if (added_cells > 0) {
      strcat(cell_list_json, ",");
    }

    strcat(cell_list_json, "\n");
    char cell_json[MAX_JSON_SIZE_CELL];
    create_cell_json_string(cell_json, cell);
    strcat(cell_list_json, cell_json);
    added_cells++;
  }

  strcat(cell_list_json, "\n  ]");
}

static void create_cell_information_response(char* json_buffer)
{
  sprintf(json_buffer, "{\n  ");

  char cell_list_json[MAX_JSON_SIZE_CELL_LIST] = "";
  create_cell_list_json_string(cell_list_json);
  strcat(json_buffer, cell_list_json);

  strcat(json_buffer, "}\n");
}
// UE JSON FUNCTIONS

static void create_ue_json_item(char* ue_json, cu_collector_ue_info_t* ue)
{
  time_t current_time = time(NULL);
  char timestamp[20];
  strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&current_time));
  sprintf(ue_json,
          "    {\n"
          "      \"rrc_state\": \"%d\",\n"
          "      \"nb_of_pdu_sessions\": \"%u\",\n"
          "      \"amf_ngap_id\": \"%u\",\n"
          "      \"ran_ngap_id\": \"%u\",\n"
          "      \"crnti\": \"%u\",\n"
          "      \"cu_f1ap_id\": \"%u\",\n"
          "      \"du_f1ap_id\": \"%u\",\n"
          "      \"reest_counter\": \"%u\",\n"
          "      \"nr_cell_id\": \"%" PRIu64
          "\",\n"
          "      \"nr_pci\": \"%u\"\n"
          "    }",
          ue->ue_state,
          ue->nb_of_pdu_sessions,
          ue->amf_ngap_id,
          ue->ran_ngap_id,
          ue->crnti,
          ue->cu_f1ap_id,
          ue->du_f1ap_id,
          ue->reest_counter,
          ue->nr_cellid,
          ue->nr_pci);
}

static void create_ue_list_json_string(char* ue_list_json)
{
  strcat(ue_list_json, "  \"ue_list\": [");
  uint8_t nb_of_ues = get_cu_collector_cell_number();
  LOG_D(FC_COLLECTOR, "CU Collector: Number of UEs: %d\n", nb_of_ues);

  if (nb_of_ues == 0) {
    strcat(ue_list_json, "\n  ]");
    return;
  }

  int added_ues = 0;

  for (int i = 0; i < COLLECTOR_UE_NUMBER; i++) {
    cu_collector_ue_info_t* ue = cu_collector_struct.ue_list[i];
    if (!ue || ue->crnti == 0) {
      continue;
    }

    if (added_ues > 0) {
      strcat(ue_list_json, ",");
    }

    strcat(ue_list_json, "\n");
    char ue_json[MAX_JSON_SIZE_UE];
    create_ue_json_item(ue_json, ue);
    strcat(ue_list_json, ue_json);
    added_ues++;
  }

  strcat(ue_list_json, "\n  ]");
}

static void create_ue_information_response(char* json_buffer)
{
  sprintf(json_buffer, "{\n  ");

  char ue_list_json[MAX_JSON_SIZE_UE_LIST] = "";
  create_ue_list_json_string(ue_list_json);
  strcat(json_buffer, ue_list_json);

  strcat(json_buffer, "}\n");
}