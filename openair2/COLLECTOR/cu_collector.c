#include <time.h>
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"

#include "cu_collector.h"
#include "intertask_interface.h"
#include "GNB_APP/gnb_config.h"
#include "COMMON/rrc_messages_types.h"
#include "RRC/NR/rrc_gNB_UE_context.h"
#include "RRC/NR/rrc_gNB_du.h"

#define MSEC_TO_SEC(miliseconds) ((miliseconds) / 1000.0)

extern RAN_CONTEXT_t RC;

uint32_t cu_collection_interval = 1000; //miliseconds
bool cu_first_time_copy_flag = false;
bool cu_collection_interval_changed_flag = false;

cu_collector_struct_t cu_collector_struct = {0};
static void cu_collector_copy_cell(gNB_RRC_INST* rrc);
static void reset_cu_collector_structure(void);
static void cu_collector_copy_ue_info(gNB_RRC_INST* rrc);


void *NR_CU_Collector_Task(void *arg) {
    MessageDef *received_msg = NULL;
    int         result;
    LOG_I(FC_COLLECTOR, "Starting CU Collector Task!\n");
    itti_mark_task_ready(TASK_CU_COLLECTOR);

    cu_collector_config_t *cfg = (cu_collector_config_t *)arg;
    LOG_I(FC_COLLECTOR, "CU Collector will create HTTP Listener on the port: %u\n", cfg->http_port);
    sleep(2);

    collector_rest_listener_args_t cu_collector_listener_args = {0};
    cu_collector_listener_args.timeInterval = &cu_collection_interval;
    cu_collector_listener_args.firstTimeCopyFlag = &cu_first_time_copy_flag;
    cu_collector_listener_args.intervalChangedFlag = &cu_collection_interval_changed_flag;
    cu_collector_listener_args.port = (uint16_t)cfg->http_port;
    cu_collector_listener_args.ip = malloc(sizeof(cfg->http_host));
    if (cu_collector_listener_args.ip != NULL) {
      strncpy(cu_collector_listener_args.ip, cfg->http_host, sizeof(cfg->http_host) - 1);
      cu_collector_listener_args.ip[sizeof(cfg->http_host) - 1] = '\0';
    }
    threadCreate(&cu_collector_listener_args.collector_listener_thread,
                 nr_collector_rest_listener,
                 (void *)&cu_collector_listener_args,
                 "CU_COLLECTOR_HTTTP",
                 -1,
                 sched_get_priority_min(SCHED_OAI) + 1);
    long timer_id;
    timer_setup((int)MSEC_TO_SEC(cu_collection_interval), 0, TASK_CU_COLLECTOR, INSTANCE_DEFAULT, TIMER_PERIODIC, NULL, &timer_id);
    LOG_I(FC_COLLECTOR, "CU Collector Task: Timer setup with id %ld\n", timer_id);

       
    while (1) {
        itti_receive_msg(TASK_CU_COLLECTOR, &received_msg);
        LOG_D(FC_COLLECTOR, "Collector Task Received %s for instance %ld\n", ITTI_MSG_NAME(received_msg), ITTI_MSG_DESTINATION_INSTANCE(received_msg));
        switch (ITTI_MSG_ID(received_msg)) {
          case TIMER_HAS_EXPIRED:{
            gNB_RRC_INST* rrc = RC.nrrrc[0];
            pthread_mutex_lock(&cu_collector_struct.cu_collector_mutex);
            reset_cu_collector_structure();
            cu_collector_copy_cell(rrc);
            cu_collector_copy_ue_info(rrc);
            pthread_mutex_unlock(&cu_collector_struct.cu_collector_mutex);
            break;
          }
          case TERMINATE_MESSAGE:
            LOG_I(FC_COLLECTOR, " *** Exiting CU Collector Thread\n");
            reset_cu_collector_structure();
            itti_exit_task();
            break;         
            default:
            LOG_E(FC_COLLECTOR, "CU Collector Received unhandled message: %d:%s\n",
                ITTI_MSG_ID(received_msg), ITTI_MSG_NAME(received_msg));
            break;
            } 

        result = itti_free (ITTI_MSG_ORIGIN_ID(received_msg), received_msg);
        AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
        received_msg = NULL;
    }

    return NULL;
}

// CELL Information Functions
static cu_collector_cell_info_t *get_cu_collector_cell_info(uint64_t nr_cell_id, bool create_new)
{
  cu_collector_cell_info_t *cell = NULL;
  int first_free_idx = -1;

  for (int cell_idx = 0; cell_idx < COLLECTOR_CELL_NUMBER; cell_idx++) {
    cu_collector_cell_info_t *tmp_cell = cu_collector_struct.cell_list[cell_idx];

    if (tmp_cell && tmp_cell->nr_cell_id == nr_cell_id) {
      return tmp_cell;
    }

    if (tmp_cell == NULL && first_free_idx == -1) {
      first_free_idx = cell_idx;
    }
  }

  if (first_free_idx != -1) {
    cell = (cu_collector_cell_info_t *)calloc(1, sizeof(cu_collector_cell_info_t));
    cu_collector_struct.cell_list[first_free_idx] = cell;
  }

  return cell;
}

static void copy_cell_information(cu_collector_cell_info_t *collector_cell,
                                  const f1ap_served_cell_info_t *cell_info,
                                  const uint32_t gnb_id)
{
  collector_cell->gnb_id = gnb_id;
  collector_cell->nr_cell_id = cell_info->nr_cellid;
  collector_cell->nr_pci = cell_info->nr_pci;
  collector_cell->dl_bandwidth = cell_info->mode == F1AP_MODE_FDD ? cell_info->fdd.dl_tbw.nrb : cell_info->tdd.tbw.nrb;
  collector_cell->ul_bandwidth = cell_info->mode == F1AP_MODE_FDD ? cell_info->fdd.ul_tbw.nrb : cell_info->tdd.tbw.nrb;
  collector_cell->dl_freq_band = cell_info->mode == F1AP_MODE_FDD ? cell_info->fdd.dl_freqinfo.band : cell_info->tdd.freqinfo.band;
  collector_cell->arfcn = cell_info->mode == F1AP_MODE_FDD ? cell_info->fdd.dl_freqinfo.arfcn : cell_info->tdd.freqinfo.arfcn;
  collector_cell->scs = cell_info->mode == F1AP_MODE_FDD ? cell_info->fdd.dl_tbw.scs : cell_info->tdd.tbw.scs;
  collector_cell->tac = cell_info->tac == NULL ? 0 : *cell_info->tac;
  collector_cell->dl_carrier_frequency_hz = from_nrarfcn(collector_cell->dl_freq_band, collector_cell->scs, collector_cell->arfcn);
}

static void cu_collector_copy_cell(gNB_RRC_INST* rrc)
{
  LOG_D(FC_COLLECTOR, "CU Collector: Copy Cell Statistics\n");
  nr_rrc_du_container_t *it = NULL;
  uint8_t cell_idx = 0;
  RB_FOREACH (it, rrc_du_tree, &rrc->dus) {
    nr_rrc_du_container_t* du = it;
    const f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[cell_idx].info;
    LOG_D(FC_COLLECTOR, "CU Collector: Copying cell %lu\n", cell_info->nr_cellid);

    cu_collector_cell_info_t *collector_cell = get_cu_collector_cell_info(cell_info->nr_cellid, true);
    if (!collector_cell) {
      LOG_E(FC_COLLECTOR, "CU Collector: Could not get cell info for cell %lu\n", du->setup_req->cell[0].info.nr_cellid);
      continue;
    }

    collector_cell->cell_state = du->cell_state;
    copy_cell_information(collector_cell, cell_info, rrc->node_id);
  }
}

void set_cu_collector_cell_state_inactive(uint64_t nr_cell_id) {
    cu_collector_cell_info_t *collector_cell = get_cu_collector_cell_info(nr_cell_id, false);
    collector_cell->cell_state = CELL_STATE_INACTIVE;
}

uint8_t get_cu_collector_cell_number(void)
{
  uint8_t nb_of_cells = 0;
  for (int cell_idx = 0; cell_idx < COLLECTOR_CELL_NUMBER; cell_idx++) {
    cu_collector_cell_info_t *tmp_cell = cu_collector_struct.cell_list[cell_idx];
    if (tmp_cell && tmp_cell->nr_cell_id != 0) {
      nb_of_cells++;
    }
  }

  return nb_of_cells;
}

void reset_cu_cell_info_list(void)
{
  for (int cell_idx = 0; cell_idx < COLLECTOR_CELL_NUMBER; cell_idx++) {
    cu_collector_cell_info_t *tmp_cell = cu_collector_struct.cell_list[cell_idx];
    if (tmp_cell) {
      free(tmp_cell);
      cu_collector_struct.cell_list[cell_idx] = NULL;
    }
  }
}

// UE Information Functions

static void cu_collector_copy_ue_info(gNB_RRC_INST* rrc)
{
    LOG_D(FC_COLLECTOR, "CU Collector: Copy UE Stats\n");
    rrc_gNB_ue_context_t *ue_context_p = NULL;
    const nr_rrc_du_container_t *du = NULL;
    uint8_t ue_idx = 0;
    RB_FOREACH(ue_context_p, rrc_nr_ue_tree_s, &(RC.nrrrc[0]->rrc_ue_head)) {
        gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
        if (!UE)
          continue;


      
        cu_collector_ue_info_t *ue_info = (cu_collector_ue_info_t*)calloc(1,sizeof(cu_collector_ue_info_t)); 
        ue_info->ue_state = UE->StatusRrc;
        ue_info->nb_of_pdu_sessions =UE->nb_of_pdusessions;
        ue_info->amf_ngap_id =UE->amf_ue_ngap_id;
        ue_info->ran_ngap_id = UE->rrc_ue_id;
        ue_info->crnti = UE->rnti;
        ue_info->cu_f1ap_id = UE->rrc_ue_id;
        ue_info->du_f1ap_id = UE->rnti;
        ue_info->reest_counter = UE->ue_reestablishment_counter;  
        ue_info->crnti = UE->rnti;

        cu_collector_struct.ue_list[ue_idx] = ue_info;
        ue_idx++;
        
        du = get_du_for_ue(rrc, UE->rrc_ue_id);
        if (!du) {
          LOG_E(FC_COLLECTOR, "CU Collector: Could not get DU for UE %u\n", UE->rnti);
          continue;
        }
        f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
        ue_info->nr_cellid = cell_info->nr_cellid;
        ue_info->nr_pci = cell_info->nr_pci;

        cu_collector_cell_info_t *cu_collector_cell = get_cu_collector_cell_info(ue_info->nr_cellid, false);
        if (cu_collector_cell) {
          if (ue_info->ue_state >= NR_RRC_CONNECTED)
            cu_collector_cell->number_of_connected_ues++;

          if (ue_info->ue_state >= NR_RRC_CONNECTED && ue_info->nb_of_pdu_sessions > 0) {
            cu_collector_cell->number_of_active_ues++;
          }
        }
    }
}

uint8_t get_cu_collector_number_of_ues(void)
{
  uint8_t nb_of_ues = 0;
  for (int ue_idx = 0; ue_idx < COLLECTOR_CELL_NUMBER; ue_idx++) {
    cu_collector_ue_info_t *tmp_ue = cu_collector_struct.ue_list[ue_idx];
    if (tmp_ue && tmp_ue->crnti != 0) {
      nb_of_ues++;
    }
  }

  return nb_of_ues;
}

void reset_cu_ue_info_list(void)
{
  for (int ue_idx = 0; ue_idx < COLLECTOR_UE_NUMBER; ue_idx++) {
    cu_collector_ue_info_t *tmp_ue = cu_collector_struct.ue_list[ue_idx];
    if (tmp_ue) {
      free(tmp_ue);
      cu_collector_struct.ue_list[ue_idx] = NULL;
    }
  }
}

static void reset_cu_collector_structure(void){
  reset_cu_cell_info_list();
  reset_cu_ue_info_list();
}