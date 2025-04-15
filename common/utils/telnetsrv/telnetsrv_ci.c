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

/*! \file telnetsrv_ci.c
 * \brief Implementation of telnet CI functions for gNB
 * \note  This file contains telnet-related functions specific to 5G gNB.
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_ue_manager.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_entity_am.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h"
#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt(mSG, ##aRGS); return 1; } while (0)

static int get_single_ue_rnti_mac(void)
{
  NR_UE_info_t *ue = NULL;
  UE_iterator(RC.nrmac[0]->UE_info.list, it) {
    if (it && ue)
      return -1;
    if (it)
      ue = it;
  }
  if (!ue)
    return -1;

  return ue->rnti;
}

rrc_gNB_ue_context_t *get_ue_by_rrc_ue_id(uint32_t rrc_ue_id)
{
  rrc_gNB_ue_context_t *ue = NULL;
  rrc_gNB_ue_context_t *l = NULL;
  RB_FOREACH (l, rrc_nr_ue_tree_s, &RC.nrrrc[0]->rrc_ue_head) {
    if (l != NULL && l->ue_context.rrc_ue_id == rrc_ue_id) {
      ue = l;
      break;
    }
  }
  return ue;
}

int get_single_rnti(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (buf)
    ERROR_MSG_RET("no parameter allowed\n");

  int rnti = get_single_ue_rnti_mac();
  if (rnti < 1)
    ERROR_MSG_RET("different number of UEs\n");

  prnt("single UE RNTI %04x\n", rnti);
  return 0;
}

int get_reestab_count(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrrrc)
    ERROR_MSG_RET("no RRC present, cannot list counts\n");
  rrc_gNB_ue_context_t *ue = NULL;
  int rnti = -1;
  if (!buf) {
    rrc_gNB_ue_context_t *l = NULL;
    int n = 0;
    RB_FOREACH(l, rrc_nr_ue_tree_s, &RC.nrrrc[0]->rrc_ue_head) {
      if (ue == NULL) ue = l;
      n++;
    }
    if (!ue)
      ERROR_MSG_RET("could not find any UE in RRC\n");
    if (n > 1)
      ERROR_MSG_RET("more than one UE in RRC present\n");
  } else {
    rnti = strtol(buf, NULL, 16);
    if (rnti < 1 || rnti >= 0xfffe)
      ERROR_MSG_RET("RNTI needs to be [1,0xfffe]\n");
    ue = rrc_gNB_get_ue_context_by_rnti_any_du(RC.nrrrc[0], rnti);
    if (!ue)
      ERROR_MSG_RET("could not find UE with RNTI %04x in RRC\n");
  }

  prnt("UE RNTI %04x reestab %d reconfig %d\n",
       ue->ue_context.rnti,
       ue->ue_context.ue_reestablishment_counter,
       ue->ue_context.ue_reconfiguration_counter);
  return 0;
}

int trigger_reestab(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrmac)
    ERROR_MSG_RET("no MAC/RLC present, cannot trigger reestablishment\n");
  int rnti = -1;
  if (!buf) {
    rnti = get_single_ue_rnti_mac();
    if (rnti < 1)
      ERROR_MSG_RET("no UE found\n");
  } else {
    rnti = strtol(buf, NULL, 16);
    if (rnti < 1 || rnti >= 0xfffe)
      ERROR_MSG_RET("RNTI needs to be [1,0xfffe]\n");
  }

  nr_rlc_test_trigger_reestablishment(rnti);

  prnt("Reset RLC counters of UE RNTI %04x to trigger reestablishment\n", rnti);
  return 0;
}

extern bool fc_trigger_integrity_failure(ue_id_t ue_id, int srb_flag, int rb_id);
/**
 * @brief Trigger Integrity Failure for UE and RB
 * @param buf: RRC UE ID, SRB FLAG, RB ID
 * @param debug: Debug flag
 * @param prnt: Print function
 * @return 0 on success, -1 on failure
 */
int fc_nr_pdcp_trigger_integrity_failure(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!buf) {
    ERROR_MSG_RET("Please provide ue id, srb flag, radio bearer id,\n");
  } else {
    // Parse ue id
    char *token = strtok(buf, ",");
    if (!token) {
      ERROR_MSG_RET("Invalid input. Expected format: ueId, srbFlag, rbId,\n");
    }
    uint32_t ueId = strtol(token, NULL, 10);

    // Parse srb flag
    token = strtok(NULL, ",");
    if (!token) {
      ERROR_MSG_RET("Missing srb flag\n");
    }
    int srbFlag = strtol(token, NULL, 10);

    // Parse rb id
    token = strtok(NULL, ",");
    if (!token) {
      ERROR_MSG_RET("missing rb id");
    }
    uint32_t rbId = strtol(token, NULL, 10);

    if (!fc_trigger_integrity_failure(ueId, srbFlag, rbId)) {
      ERROR_MSG_RET("could not trigger integrity failure for this UE with ue_id %d in PDCP\n", ueId);
    } else {
      prnt("Integrity failure triggered for UE %u with srb flag %d and rb id %u\n", ueId, srbFlag, rbId);
    }
  }
  return 0;
}

extern bool fc_manipulate_rx_deliv_count(ue_id_t ue_id, int srb_flag, int rb_id);
/**
 * @brief Manipoulates rx deliv count UE and RB which causes pdcp failure
 * @param buf: RRC UE ID, SRB FLAG, RB ID
 * @param debug: Debug flag
 * @param prnt: Print function
 * @return 0 on success, -1 on failure
 */
int fc_nr_pdcp_manipulate_rx_deliv_count(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!buf) {
    ERROR_MSG_RET("Please provide ue id, srb flag, radio bearer id,\n");
  } else {
    // Parse ue id
    char *token = strtok(buf, ",");
    if (!token) {
      ERROR_MSG_RET("Invalid input. Expected format: ueId, srbFlag, rbId,\n");
    }
    uint32_t ueId = strtol(token, NULL, 10);

    // Parse srb flag
    token = strtok(NULL, ",");
    if (!token) {
      ERROR_MSG_RET("Missing srb flag\n");
    }
    int srbFlag = strtol(token, NULL, 10);

    // Parse rb id
    token = strtok(NULL, ",");
    if (!token) {
      ERROR_MSG_RET("missing rb id");
    }
    uint32_t rbId = strtol(token, NULL, 10);

    if (!fc_manipulate_rx_deliv_count(ueId, srbFlag, rbId)) {
      ERROR_MSG_RET("could not trigger manipulate_recvd_cnt for this UE with ue_id %d in PDCP\n", ueId);
    } else {
      prnt("manipulate_recvd_cnt for UE %u with srb flag %d and rb id %u\n", ueId, srbFlag, rbId);
    }
  }
  return 0;
}
extern void nr_HO_N2_trigger(gNB_RRC_INST *rrc, uint32_t neighbour_phy_cell_id, uint32_t serving_phy_cell_id, uint32_t rrc_ue_id);
/**
 * @brief Trigger N2 handover for UE
 * @param buf: RRC UE ID, Neighbour PHY CELL ID, Serving PHY CELL ID
 * @param debug: Debug flag
 * @param prnt: Print function
 * @return 0 on success, -1 on failure
 */
int rrc_gNB_trigger_n2_ho(char *buf, int debug, telnet_printfunc_t prnt)
{
  if (!RC.nrrrc)
    ERROR_MSG_RET("no RRC present, cannot list counts\n");

  if (!buf) {
    ERROR_MSG_RET("Please provide serving cell id, neighbour cell id, and ue id\n");
  } else {
    // Parse servingCellId
    char *token = strtok(buf, ",");
    if (!token) {
      ERROR_MSG_RET("Invalid input. Expected format: servingCellId,neighbourCellId,ueId\n");
    }
    uint32_t servingCellId = strtol(token, NULL, 10);

    // Parse neighbourCellId
    token = strtok(NULL, ",");
    if (!token) {
      ERROR_MSG_RET("Missing neighbour cell id\n");
    }
    uint32_t neighbourCellId = strtol(token, NULL, 10);

    // Parse ueId
    token = strtok(NULL, ",");
    if (!token) {
      ERROR_MSG_RET("Missing ue id\n");
    }
    uint32_t ueId = strtol(token, NULL, 10);
    rrc_gNB_ue_context_t *ue_p = get_ue_by_rrc_ue_id(ueId);
    if (!ue_p) {
      ERROR_MSG_RET("UE with id %u not found\n", ueId);
    }
    if (ue_p->ue_context.StatusRrc < NR_RRC_CONNECTED || ue_p->ue_context.nb_of_pdusessions < 1)
      ERROR_MSG_RET("could not trigger HO for this UE with ue_id %d in RRC\n", ue_p->ue_context.rrc_ue_id);

    gNB_RRC_UE_t *UE = &ue_p->ue_context;
    nr_HO_N2_trigger(RC.nrrrc[0], neighbourCellId, servingCellId, UE->rrc_ue_id);
    prnt("RRC N2 handover triggered for UE %u with serving cell id %u, neighbour cell id %u\n",
         ueId,
         servingCellId,
         neighbourCellId);
  }

  return 0;
}

static telnetshell_cmddef_t cicmds[] = {
    {"get_single_rnti", "", get_single_rnti},
    {"force_reestab", "[rnti(hex,opt)]", trigger_reestab},
    {"get_reestab_count", "[rnti(hex,opt)]", get_reestab_count},
    {"nr_N2_HO_trigger", "[servingCellId(uint32_t),neighbourCellId(uint32_t),ueId(uint32_t)]", rrc_gNB_trigger_n2_ho},
    {"trigger_intg_fail", "[ueId(uint32_t),srbFlag(int),rbId(uint32_t)]", fc_nr_pdcp_trigger_integrity_failure},
    {"manpl_recv_cnt", "[ueId(uint32_t),srbFlag(int),rbId(uint32_t)]", fc_nr_pdcp_manipulate_rx_deliv_count},
    {"", "", NULL},
};

static telnetshell_vardef_t civars[] = {

  {"", 0, 0, NULL}
};

void add_ci_cmds(void) {
  add_telnetcmd("ci", civars, cicmds);
}
