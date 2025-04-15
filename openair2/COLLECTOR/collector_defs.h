#ifndef COLLECTOR_DEFS_H_
#define COLLECTOR_DEFS_H_

#include "common/openairinterface5g_limits.h"
#include "common/ran_context.h"
#include "RRC/NR/nr_rrc_defs.h"


#define COLLECTOR_UE_NUMBER MAX_MOBILES_PER_GNB
#define COLLECTOR_CELL_NUMBER 6

// CU Collector Definitions

typedef struct cu_collector_config {
  uint16_t http_port;
  uint16_t telnet_port;
  char http_host[16];
  char telnet_host[16];
} cu_collector_config_t;

typedef struct cu_collector_ue_info {
NR_UE_STATE_t ue_state;
uint8_t nb_of_pdu_sessions;
uint32_t amf_ngap_id;
uint32_t ran_ngap_id;
uint32_t crnti;
uint32_t cu_f1ap_id;
uint32_t du_f1ap_id;
uint32_t reest_counter; 
uint64_t nr_cellid; 
uint16_t nr_pci;
} cu_collector_ue_info_t; 

typedef struct cu_collector_cell_info {
uint16_t dl_bandwidth;
uint16_t ul_bandwidth;
uint8_t scs; 
int dl_freq_band;
uint32_t tac;
uint64_t nr_cell_id;
uint16_t nr_pci;
uint64_t dl_carrier_frequency_hz;
uint32_t arfcn;
cell_state_t cell_state;
uint8_t number_of_connected_ues;
uint8_t number_of_active_ues;
uint32_t gnb_id;
} cu_collector_cell_info_t;

typedef struct cu_collector_struct_t {
  cu_collector_ue_info_t *ue_list[COLLECTOR_UE_NUMBER];
  cu_collector_cell_info_t *cell_list[COLLECTOR_CELL_NUMBER];
  pthread_mutex_t cu_collector_mutex;
} cu_collector_struct_t;

// DU Collector Definitions


typedef struct du_collector_config {
  uint16_t http_port;
  uint16_t redis_port;
  char http_host[16];
  char redis_host[16];
} du_collector_config_t;

typedef struct du_collector_mac_stats {
    uint32_t dl_total_rbs;
    uint32_t ul_total_rbs;
    uint64_t dl_total_bytes;
    uint64_t ul_total_bytes;
} du_collector_mac_stats_t; 

typedef struct du_collector_ue_stats {
    //identity parameters
    uint16_t crnti;
    uint32_t cu_f1ap_id;
    uint32_t du_f1ap_id;

    //calculation for throughput and total rbs
    du_collector_mac_stats_t current_stats;
    du_collector_mac_stats_t previous_stats;

    //ue statistics
    // uint64_t dl_total_rbs;
    // uint64_t ul_total_rbs;
    float ul_throughput;
    float dl_throughput;

} du_collector_ue_stats_t;

typedef struct du_collector_cell_stats {
    //identity parameters
    uint64_t cell_id;
    uint16_t cell_pci;
    uint64_t du_id;

    //calculation for throughput and prb usage
    du_collector_mac_stats_t current_stats;
    du_collector_mac_stats_t previous_stats;

    //cell statistics
    float                           dl_prb_usg;
    float                           ul_prb_usg;
    float                           dl_throughput;
    float                           ul_throughput;
} du_collector_cell_stats_t;

typedef struct du_collector_struct {
  pthread_mutex_t du_collector_mutex;
  du_collector_ue_stats_t *ue_list[COLLECTOR_UE_NUMBER];
  du_collector_cell_stats_t *cell_list[COLLECTOR_CELL_NUMBER];
  pthread_t collector_thread;
} du_collector_struct_t;

#endif 