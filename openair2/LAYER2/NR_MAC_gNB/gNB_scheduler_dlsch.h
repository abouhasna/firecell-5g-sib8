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

void get_start_stop_allocation(gNB_MAC_INST *mac,
                               NR_UE_info_t *UE,
                               int *rbStart,
                               int *rbStop);

bool allocate_dl_retransmission(module_id_t module_id,
                                frame_t frame,
                                sub_frame_t slot,
                                uint16_t *rballoc_mask,
                                int *n_rb_sched,
                                NR_UE_info_t *UE,
                                int current_harq_pid);

typedef struct UEsched_s {
  /// Proportional-fair priority coefficient
  float coef;
  /// QoS-aware priority coefficient
  float qpcoeff;
  NR_UE_info_t * UE;
} UEsched_t;

#endif