#include <time.h>
#include <hiredis/hiredis.h>
#include "common/utils/LOG/log.h"
#include "intertask_interface.h"


#include "du_collector.h"
#include "collector_rest_manager.h"
#include "collector_db_manager.h"
#include "f1ap_ids.h"
#include "f1ap_common.h"

#include "LAYER2/NR_MAC_COMMON/nr_mac_extern.h"
#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "GNB_APP/gnb_config.h"
#include "COMMON/rrc_messages_types.h"
#include "RRC/NR/rrc_gNB_UE_context.h"
#include "PHY/defs_gNB.h"
#include "LAYER2/nr_pdcp/nr_pdcp.h"
#include "RRC/NR/nr_rrc_defs.h"

#define REDIS_PORT 6379
#define REDIS_HOST "127.0.0.1"
uint32_t du_http_port = 6090;

#define BYTES_TO_KB(bytes) ((bytes) / 1024.0)
#define MSEC_TO_SEC(miliseconds) ((miliseconds) / 1000.0)

const int moduleP = 0;
const int CC_id = 0;
const int copyStatsOption = 0;
uint32_t collectionInterval = 1000; //miliseconds
#define frameInterval ((int) (collectionInterval / 10))
bool firstTimeCopyFlag = false;
bool intervalChangedFlag = false;

du_collector_struct_t du_collector_struct_current = {0};
du_collector_struct_t du_collector_struct_previous = {0};
extern RAN_CONTEXT_t RC;

redisContext* redis;


bool is_rnti_matched(const void* ueP, rnti_t rnti){
    const du_collector_ue_stats_t * ue = (const du_collector_ue_stats_t *) ueP;
    return (ue->crnti == rnti);
}

int countSetBits(uint64_t n) {
    int count = 0;
    while(n != 0) {
        n = n & (n - 1); // clear least significant 1-bit
        count++;
    }
    return count;
}


du_collector_ue_stats_t* find_ue_by_rnti(du_collector_struct_t* du_collector_struct, rnti_t rnti)
{
    DU_Collector_UE_Iterator(du_collector_struct->ue_list, UE){
        if (is_rnti_matched(UE, rnti)) {
            return UE;
        }
    }
    return NULL;
}

void set_ues_mac_statistics(du_collector_ue_stats_t *UE_c, NR_mac_stats_t *stats, du_collector_ue_stats_t *UE_prev)
{   
    UE_c->current_stats.dl_total_rbs     = stats->dl.total_rbs   - UE_prev->previous_stats.dl_total_rbs;
    UE_c->current_stats.ul_total_rbs     = stats->ul.total_rbs   - UE_prev->previous_stats.ul_total_rbs;
    UE_c->current_stats.dl_total_bytes = stats->dl.total_bytes - UE_prev->previous_stats.dl_total_bytes;
    UE_c->current_stats.ul_total_bytes = stats->ul.total_bytes - UE_prev->previous_stats.ul_total_bytes;
}

void set_ues_initial_mac_statistics(du_collector_ue_stats_t *UE_c, NR_mac_stats_t *stats)
{
    UE_c->current_stats.dl_total_rbs     = stats->dl.total_rbs;
    UE_c->current_stats.ul_total_rbs     = stats->ul.total_rbs;
    UE_c->current_stats.dl_total_bytes = stats->dl.total_bytes;
    UE_c->current_stats.ul_total_bytes = stats->ul.total_bytes;
}

void set_ues_previous_mac_statistics(du_collector_ue_stats_t *UE_c, NR_mac_stats_t *stats)
{
    UE_c->previous_stats.dl_total_rbs   = stats->dl.total_rbs;
    UE_c->previous_stats.ul_total_rbs   = stats->ul.total_rbs;
    UE_c->previous_stats.dl_total_bytes = stats->dl.total_bytes;
    UE_c->previous_stats.ul_total_bytes = stats->ul.total_bytes;
}

void reset_mac_collector(du_collector_struct_t* du_collector_struct)
{
    for (uint8_t i = 0; i < COLLECTOR_CELL_NUMBER; i++){
        if (du_collector_struct->cell_list[i] != NULL) {
            free(du_collector_struct->cell_list[i]);
            du_collector_struct->cell_list[i] = NULL;
        }
    }
    for (uint8_t i = 0; i < COLLECTOR_UE_NUMBER; i++){
        if (du_collector_struct->ue_list[i] != NULL) {
            free(du_collector_struct->ue_list[i]);
            du_collector_struct->ue_list[i] = NULL;
        }
    }
}

void copy_mac_stats(gNB_MAC_INST * gNB, du_collector_struct_t * collectorStruct, uint8_t CC_id) {
    // collectionInterval / 10 = number of frames.
    //calculation of number of prbs in given time interval.
    //act like single cell
    // const uint64_t current_tracking_are_code = RC.nrrrc[moduleP]->configuration.tac;
    // -- configuration variables --
    const PHY_VARS_gNB* const phyInst = RC.gNB[moduleP];
    const int DlPrbsInSlot = phyInst->frame_parms.N_RB_DL;
    const int UlPrbsInSlot = phyInst->frame_parms.N_RB_UL;
    // const int tempScs = phyInst->frame_parms.subcarrier_spacing;
    // const uint64_t dlCarrierFreq = phyInst->frame_parms.dl_CarrierFreq;
    const int slotsPerSubFrame = phyInst->frame_parms.slots_per_subframe;

    // const nfapi_nr_config_request_scf_t * const cfg = &gNB->config[CC_id];
    // const uint16_t dlBandwidth = cfg->carrier_config.dl_bandwidth.value;
    // const uint16_t ulBandwidth = cfg->carrier_config.uplink_bandwidth.value;

    const uint32_t slotsInCollectionInterval = slotsPerSubFrame * 10 * (collectionInterval/10);

    const float dlPrbs = DlPrbsInSlot * slotsInCollectionInterval;
    const float ulPrbs = UlPrbsInSlot * slotsInCollectionInterval;
    uint8_t dlSlotsInSubframe = countSetBits(gNB->dlsch_slot_bitmap[0]);
    uint8_t ulSlotsInSubframe = countSetBits(gNB->ulsch_slot_bitmap[0]);

    float dlPrbsInTotal   = dlPrbs * (float)dlSlotsInSubframe/(slotsPerSubFrame*10);
    float ulPrbsInTotal   = ulPrbs * (float)ulSlotsInSubframe/(slotsPerSubFrame*10);

    // LOG_D(NR_RRC, "\ndlSlotsInSubframe: %u ulSlotsInSubframe: %u \ndlPrbsInTotal: %f ulPrbsInTotal: %f\n",dlSlotsInSubframe,ulSlotsInSubframe,dlPrbsInTotal,ulPrbsInTotal);

    // // -- collecting cell level statistics --
    du_collector_cell_stats_t* CC_c = (du_collector_cell_stats_t*)calloc(1, sizeof(du_collector_cell_stats_t));
    collectorStruct->cell_list[CC_id] = CC_c;
    CC_c->cell_id = gNB->f1_config.setup_req->cell[0].info.nr_cellid;
    CC_c->cell_pci = gNB->f1_config.setup_req->cell[0].info.nr_pci;
    CC_c->du_id = gNB->f1_config.setup_req[0].gNB_DU_id;

    // CC_c->carrierId = CC_id;
    // CC_c->slotsPassed = slotsInCollectionInterval;
    // CC_c->timePassed = collectionInterval;
    // CC_c->ulBw = ulBandwidth;
    // CC_c->dlBw = dlBandwidth;
    // CC_c->dlCarrierFreq = dlCarrierFreq;
    // CC_c->subcarrierSpacing = (uint8_t)(tempScs / 1000);
    
    pthread_mutex_lock(&gNB->UE_info.mutex);
    int ueNumber = 0;
    UE_iterator(gNB->UE_info.list, UE) {
        NR_mac_stats_t *stats = &UE->mac_stats;
        du_collector_ue_stats_t *UE_c = (du_collector_ue_stats_t *)calloc(1,sizeof(du_collector_ue_stats_t));
        //identity parameters
        UE_c->crnti      = UE->rnti;
        if (!du_exists_f1_ue_data(UE_c->crnti))
          continue;

        f1_ue_data_t ue_data = du_get_f1_ue_data(UE_c->crnti);

        UE_c->du_f1ap_id = UE_c->crnti;
        UE_c->cu_f1ap_id = ue_data.secondary_ue;

        collectorStruct->ue_list[ueNumber] = UE_c;
        du_collector_ue_stats_t *UE_prev = find_ue_by_rnti(&du_collector_struct_previous, UE_c->crnti);

        if (!firstTimeCopyFlag && UE_prev != NULL) {
            set_ues_mac_statistics(UE_c, stats, UE_prev);
        } else {
            set_ues_initial_mac_statistics(UE_c, stats);
        }
        set_ues_previous_mac_statistics(UE_c, stats);

        UE_c->ul_throughput = BYTES_TO_KB(UE_c->current_stats.ul_total_bytes) / MSEC_TO_SEC(collectionInterval);
        UE_c->dl_throughput = BYTES_TO_KB(UE_c->current_stats.dl_total_bytes) / MSEC_TO_SEC(collectionInterval);
        LOG_D(FC_COLLECTOR, "Ul throughput for UE Cu Id: %u is : %f Kbps\n", UE_c->cu_f1ap_id, UE_c->ul_throughput);
        LOG_D(FC_COLLECTOR, "Dl throughput for UE Cu Id: %u is : %f Kbps\n", UE_c->cu_f1ap_id, UE_c->dl_throughput);

        //Carrier Statistics.
        CC_c->current_stats.dl_total_rbs   += UE_c->current_stats.dl_total_rbs;
        CC_c->current_stats.ul_total_rbs   += UE_c->current_stats.ul_total_rbs;
        CC_c->current_stats.dl_total_bytes += UE_c->current_stats.dl_total_bytes;
        CC_c->current_stats.ul_total_bytes += UE_c->current_stats.ul_total_bytes;

        CC_c->dl_throughput += UE_c->dl_throughput;
        CC_c->ul_throughput += UE_c->ul_throughput;
        LOG_D(FC_COLLECTOR, "\nStatistics for ue: %d \n dl_total_rbs: %d \t ul_total_rbs: %d\n",
        ueNumber, UE_c->current_stats.dl_total_rbs, UE_c->current_stats.ul_total_rbs);

        ueNumber++;
        
    }
    pthread_mutex_unlock(&gNB->UE_info.mutex);
    LOG_D(FC_COLLECTOR, "\nStatistics for cell: \n dl_total_rbs: %d \t ul_total_rbs: %d\n", CC_c->current_stats.dl_total_rbs, CC_c->current_stats.ul_total_rbs);
    float dlUsage = CC_c->current_stats.dl_total_rbs / dlPrbsInTotal;
    float ulUsage = CC_c->current_stats.ul_total_rbs / ulPrbsInTotal;
    if ((dlUsage > 1.0)){
        LOG_E(FC_COLLECTOR, "\nDL Resource Usage Error: dlUsage: %f\n", dlUsage);
        dlUsage = 1.0;
    }

    if ((ulUsage > 1.0)){
        LOG_E(FC_COLLECTOR, "\nUL Resource Usage Error: ulUsage: %f\n", ulUsage);
        ulUsage = 1.0;
    }

    CC_c->dl_prb_usg = dlUsage;
    CC_c->ul_prb_usg = ulUsage;
    LOG_D(FC_COLLECTOR, "\nCOLLECTOR USAGE: \n dl: %f \t ul: %f\n", CC_c->dl_prb_usg, CC_c->ul_prb_usg);
    LOG_D(FC_COLLECTOR, "Dl throughput is : %f \n Collection Interval is %f seconds\n", CC_c->dl_throughput, (float)collectionInterval/1000);

    firstTimeCopyFlag = false;
    reset_mac_collector(&du_collector_struct_previous);
}

void copy_stats(uint8_t CC_id)
{
    copy_mac_stats(RC.nrmac[0], &du_collector_struct_current, CC_id);
}

void nr_collector_handle_subframe_process(int frame, int subframe, int CC_id){
  LOG_D(FC_COLLECTOR, "WRITE DB TASK HAS CAME : frame: %d subframe: %d !\n", frame, subframe);
  copy_stats(CC_id);
  insert_nr_mac_connector_struct_to_redis_database(&redis, &du_collector_struct_current);
  reset_mac_collector(&du_collector_struct_current);
}

void *NR_DU_Collector_Task(void *arg) {
    MessageDef *received_msg = NULL;
    int         result;
    LOG_I(FC_COLLECTOR, "Starting Du Collector Task!\n");
    itti_mark_task_ready(TASK_DU_COLLECTOR);
    // Reading the configuration!
    du_collector_config_t *cfg = (du_collector_config_t *)arg;
    LOG_I(FC_COLLECTOR, "DU Collector will create HTTP Listener on the port: %u\n", cfg->http_port);

    int ret = connect_redis_database(&redis, cfg->redis_host, cfg->redis_port);
    if (ret == 0)
      LOG_I(FC_COLLECTOR, "DU COLLECTOR:  REDIS DATABASE CONNECTED SUCCESSFULLY! \n");

    sleep(2);
    collector_rest_listener_args_t du_collector_listener_args = {0};
    du_collector_listener_args.timeInterval = &collectionInterval;
    du_collector_listener_args.firstTimeCopyFlag = &firstTimeCopyFlag;
    du_collector_listener_args.intervalChangedFlag = &intervalChangedFlag;
    du_collector_listener_args.port = (uint16_t)cfg->http_port;
    du_collector_listener_args.ip = malloc(sizeof(cfg->http_host));
    if (du_collector_listener_args.ip != NULL) {
      strncpy(du_collector_listener_args.ip, cfg->http_host, sizeof(cfg->http_host) - 1);
      du_collector_listener_args.ip[sizeof(cfg->http_host) - 1] = '\0';
    }
    threadCreate(&du_collector_listener_args.collector_listener_thread,
                 nr_collector_rest_listener,
                 (void *)&du_collector_listener_args,
                 "DU_COLLECTOR_HTTTP",
                 -1,
                 sched_get_priority_min(SCHED_OAI) + 1);
    while (1) {
        itti_receive_msg(TASK_DU_COLLECTOR, &received_msg);
        LOG_D(FC_COLLECTOR, "Du Collector Task Received %s for instance %ld\n", ITTI_MSG_NAME(received_msg), ITTI_MSG_DESTINATION_INSTANCE(received_msg));
        switch (ITTI_MSG_ID(received_msg)) {
            case TERMINATE_MESSAGE: {
                LOG_I(FC_COLLECTOR, " *** Exiting Du Collector thread\n");
                itti_exit_task();
            }
            break;
            case RRC_SUBFRAME_PROCESS:
            {
                const int c_frame = (int) RRC_SUBFRAME_PROCESS(received_msg).ctxt.frame;
                const int c_subframe = (int) RRC_SUBFRAME_PROCESS(received_msg).ctxt.subframe;
                const int CC_id = (int) RRC_SUBFRAME_PROCESS(received_msg).CC_id;

                LOG_D(FC_COLLECTOR, "DU COLLECTOR task is starting on frame: %d subframe: %d\n", c_frame, c_subframe);
                nr_collector_handle_subframe_process(c_frame, c_subframe, CC_id);
            } break;               
            default:
            LOG_E(FC_COLLECTOR, "DU COLLECTOR Received unhandled message: %d:%s\n",
                ITTI_MSG_ID(received_msg), ITTI_MSG_NAME(received_msg));
            break;
            } 

        result = itti_free (ITTI_MSG_ORIGIN_ID(received_msg), received_msg);
        AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
        received_msg = NULL;
    }

    return NULL;
}

void (*nr_collector_trigger)(int, int, int) = NULL;

void nr_collector_trigger_func(int CC_id, int frame, int subframe)
{
  static uint16_t collectorTick = 0;
  collectorTick++;

  if (collectorTick != collectionInterval)
    return;

  collectorTick = 0;
  MessageDef *message_p;
  message_p = itti_alloc_new_message(TASK_DU_COLLECTOR, INSTANCE_DEFAULT, RRC_SUBFRAME_PROCESS);
  RRC_SUBFRAME_PROCESS(message_p).ctxt.instance = INSTANCE_DEFAULT;
  RRC_SUBFRAME_PROCESS(message_p).ctxt.subframe = subframe;
  RRC_SUBFRAME_PROCESS(message_p).ctxt.frame = frame;
  RRC_SUBFRAME_PROCESS(message_p).CC_id = CC_id;
  itti_send_msg_to_task(TASK_DU_COLLECTOR, INSTANCE_DEFAULT, message_p);
}

void nr_collector_trigger_fake(int CC_id, int frame, int subframe)
{
  return;
}

void *get_nr_mac_ue_by_rnti(uint16_t rnti)
{
  NR_UE_info_t *_ue = NULL;
  gNB_MAC_INST *nrmac = RC.nrmac[0];
  pthread_mutex_lock(&nrmac->UE_info.mutex);
  UE_iterator (nrmac->UE_info.list, UE) {
    if (UE->rnti == rnti) {
      _ue = UE;
      break;
    }
  }
  pthread_mutex_unlock(&nrmac->UE_info.mutex);
  return _ue;
}

void du_release_all_ues(void)
{
  LOG_I(FC_COLLECTOR, "Releasing all UEs from DU only if any!\n");
  gNB_MAC_INST *nrmac = RC.nrmac[0];
  pthread_mutex_lock(&nrmac->UE_info.mutex);
  UE_iterator (nrmac->UE_info.list, UE) {
    LOG_I(FC_COLLECTOR, "Releasing UE RNTI: %d from DU only!\n", UE->rnti);
    nr_mac_trigger_release_timer(&UE->UE_sched_ctrl, 1);
  }
  pthread_mutex_unlock(&nrmac->UE_info.mutex);
}
