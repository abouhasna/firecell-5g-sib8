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

#ifndef RRC_GNB_DU_H_
#define RRC_GNB_DU_H_

#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdint.h>
#include <stdio.h>

struct f1ap_setup_req_s;
struct f1ap_lost_connection_t;
struct gNB_RRC_INST_s;
struct nr_rrc_du_container_t;
struct f1ap_gnb_du_configuration_update_s;
struct f1ap_served_cell_info_t;

void rrc_gNB_process_f1_setup_req(struct f1ap_setup_req_s *req, sctp_assoc_t assoc_id);
void rrc_CU_process_f1_lost_connection(struct gNB_RRC_INST_s *rrc, struct f1ap_lost_connection_t *lc, sctp_assoc_t assoc_id);
void rrc_gNB_process_f1_du_configuration_update(struct f1ap_gnb_du_configuration_update_s *conf_up, sctp_assoc_t assoc_id);

struct nr_rrc_du_container_t *get_du_for_ue(struct gNB_RRC_INST_s *rrc, uint32_t ue_id);
struct nr_rrc_du_container_t *get_du_by_assoc_id(struct gNB_RRC_INST_s *rrc, sctp_assoc_t assoc_id);
struct nr_rrc_du_container_t* get_du_for_nr_cell_id(struct gNB_RRC_INST_s *rrc, uint64_t cellId, uint8_t* cellIdx);
const struct f1ap_served_cell_info_t *get_cell_information_by_phycellId(int phyCellId);

void dump_du_info(const struct gNB_RRC_INST_s *rrc, FILE *f);

int get_dl_band(const struct f1ap_served_cell_info_t *cell_info);
int get_ssb_scs(const struct f1ap_served_cell_info_t *cell_info);
int get_ssb_arfcn(const struct nr_rrc_du_container_t *du);

/* Firecell - Added for Cu Collector Task Purposes*/
int du_compare(const struct nr_rrc_du_container_t *a, const struct nr_rrc_du_container_t *b);
RB_PROTOTYPE(rrc_du_tree, nr_rrc_du_container_t, entries, du_compare);

#endif /* RRC_GNB_DU_H_ */
