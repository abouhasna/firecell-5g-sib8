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

#ifndef DLSCH_SCHEDULER_H
#define DLSCH_SCHEDULER_H

#include "NR_MAC_gNB/nr_mac_gNB.h"


bool allocate_ul_retransmission(gNB_MAC_INST *nrmac,
				       frame_t frame,
				       sub_frame_t slot,
				       uint16_t *rballoc_mask,
				       int *n_rb_sched,
				       NR_UE_info_t* UE,
				       int harq_pid,
				       const NR_ServingCellConfigCommon_t *scc,
				       const int tda);
void update_ul_ue_R_Qm(int mcs, int mcs_table, const NR_PUSCH_Config_t *pusch_Config, uint16_t *R, uint8_t *Qm);


int nr_ue_max_mcs_min_rb(int mu,
                                 int ph_limit,
                                 NR_sched_pusch_t *sched_pusch,
                                 NR_UE_UL_BWP_t *ul_bwp,
                                 uint16_t minRb,
                                 uint32_t tbs,
                                 uint16_t *Rb,
                                 uint8_t *mcs);

bool nr_UE_is_to_be_scheduled(uint8_t last_ul_slot,
                              const tdd_config_t *tdd_config,
                              int CC_id,
                              NR_UE_info_t *UE,
                              frame_t frame,
                              sub_frame_t slot,
                              uint32_t ulsch_max_frame_inactivity);

extern uint32_t ul_pf_tbs[3][29]; 
typedef struct UEsched_s {
  float coef;
  /// QoS-aware priority coefficient
  float qpcoeff;
  NR_UE_info_t * UE;
} UEsched_t;

#endif