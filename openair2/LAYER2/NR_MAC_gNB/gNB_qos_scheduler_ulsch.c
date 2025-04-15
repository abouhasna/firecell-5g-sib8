/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file       gNB_qos_scheduler_ulsch.c.c
 * \brief       QoS aware based scheduling
 * \author      Vijay chadachan
 * \date        2024
 * \email:      vijay.chadachan@firecell.io
 * \version     1.0
 * @ingroup     _mac

 */

#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "executables/softmodem-common.h"
#include "common/utils/nr/nr_common.h"
#include "utils.h"
#include <openair2/UTIL/OPT/opt.h>
#include "LAYER2/NR_MAC_COMMON/nr_mac_extern.h"
#include "LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "LAYER2/RLC/rlc.h"
#include "LAYER2/NR_MAC_gNB/gNB_scheduler_ulsch.h"


/**
 * @brief This function will iterate through the list of unsorted UEs (UE_list) and:
 *        - schedule retransmissions first
 *        - update number of remaining UEs to be scheduled
 *        - populate the UE_sched array with UEs to be scheduled
 */
static void schedule_retx(NR_Scheduling_context_t *sched_context, UEsched_t *UE_sched){
 }

/**
 * @brief Update uplink BWF based on the current RX SDU processing round
 */
void nr_update_uplink_bwf(uint8_t rx_lcid, NR_UE_sched_ctrl_t *sched_ctrl)
{
}

/**
 * @brief QoS-aware comparator
*/
static int bwf_comparator(const void *p, const void *q)
{
  return 0;
}

/**
 * @brief Comparator based on QoS-aware priority coefficient
 */
static int bwf_comparator2(const void *p, const void *q)
{
  return 0;
}

static void qos_ul(NR_Scheduling_context_t *sched_context, UEsched_t *iterator)
{
}

/**
 * @brief QoS-aware scheduling algorithms
*/
void nr_fr1_ulsch_qos(NR_Scheduling_context_t *sched_context) {
}
