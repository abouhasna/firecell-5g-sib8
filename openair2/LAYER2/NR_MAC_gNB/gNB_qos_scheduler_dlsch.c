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

/*! \file       gNB_qos_scheduler_dlsch.c.c
 * \brief       QoS aware based scheduling
 * \author      Guido Casati
 * \date        2024
 * \email:      hello@guidocasati.com
 * \version     1.0
 * @ingroup     _mac

 */

#include "LAYER2/NR_MAC_gNB/mac_proto.h"
#include "LAYER2/RLC/rlc.h"
#include "NR_MAC_gNB/gNB_scheduler_dlsch.h"

/**
 * @brief This function will iterate through the list of unsorted UEs (UE_list) and:
 *        - schedule retransmissions first
 *        - update number of remaining UEs to be scheduled
 *        - populate the UE_sched array with UEs to be scheduled
 */
static void schedule_retx(NR_Scheduling_context_t *sched_context, UEsched_t *UE_sched){
}

/**
 * @brief Store DLSCH buffer from RLC and fill UE Scheduling Controller:
 *        - set the UE BWF to the highest among all LCs
 */
static void nr_store_dlsch_buffer_qos(module_id_t module_id, frame_t frame, sub_frame_t slot)
{
}

/**
 * @brief  QoS-aware comparator for LCs
 * @return The difference between the BWFs of q and p
 *         ret < 0: p should come before q
 *         0:       elements are equivalent
 *         ret > 0: p should come after q
*/
int lc_comparator(const void *p, const void *q)
{
  return 0;
}

/**
 * @brief QoS-aware comparator
 * @return The difference between the computed coefficients of p and q
 *         ret < 0: p should come before q
 *         0:       elements are equivalent
 *         ret > 0: p should come after q
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

/**
 * @brief Allocates DL resources (RBs) to UEs in sorted list, until it fits
 */
static void qos_aware_dl_alloc(NR_Scheduling_context_t *sched_context, UEsched_t *iterator)
{
}

/**
 * @brief QoS-aware scheduling algorithms
*/
void nr_fr1_dlsch_qos(NR_Scheduling_context_t *sched_context) {
 }

