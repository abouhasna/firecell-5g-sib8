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

/*! \file ngap_gNB_context_management_procedures.c
 * \brief NGAP context management procedures
 * \author  Yoshio INOUE, Masayuki HARADA
 * \date 2020
 * \email: yoshio.inoue@fujitsu.com,masayuki.harada@fujitsu.com (yoshio.inoue%40fujitsu.com%2cmasayuki.harada%40fujitsu.com)
 * \version 1.0
 * @ingroup _ngap
 */
 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "assertions.h"
#include "conversions.h"

#include "intertask_interface.h"

#include "ngap_common.h"
#include "ngap_gNB_defs.h"

#include "ngap_gNB_itti_messaging.h"

#include "ngap_gNB_encoder.h"
#include "ngap_gNB_decoder.h"
#include "ngap_gNB_nnsf.h"
#include "ngap_gNB_ue_context.h"
#include "ngap_gNB_nas_procedures.h"
#include "ngap_gNB_management_procedures.h"
#include "ngap_gNB_context_management_procedures.h"
#include "NGAP_PDUSessionResourceItemCxtRelReq.h"
#include "NGAP_PDUSessionResourceItemCxtRelCpl.h"
#include "NGAP_PDUSessionResourceItemHORqd.h"
#include "NGAP_HandoverRequiredTransfer.h"
#include "NGAP_SourceNGRANNode-ToTargetNGRANNode-TransparentContainer.h"
#include "NGAP_PDUSessionResourceInformationList.h"
#include "NGAP_PDUSessionResourceInformationItem.h"
#include "NGAP_QosFlowInformationList.h"
#include "NGAP_QosFlowInformationItem.h"
#include "NGAP_LastVisitedCellItem.h"
#include "NGAP_UEHistoryInformation.h"
#include "NGAP_LastVisitedNGRANCellInformation.h"
#include "NGAP_Cause.h"
#include "NGAP_PDUSessionResourceAdmittedList.h"
#include "NGAP_PDUSessionResourceAdmittedItem.h"
#include "NGAP_HandoverRequestAcknowledgeTransfer.h"
#include "NGAP_QosFlowItemWithDataForwarding.h"
#include "NGAP_HandoverNotify.h"
#include "NGAP_HandoverCancel.h"
#include "openair2/helper/nr_ue_release_defs.h"


#define MAX_UINT32_RANGE 0xFFFFFFFF

static void allocAddrCopy(BIT_STRING_t *out, transport_layer_addr_t in)
{
  if (in.length) {
    out->buf = malloc(in.length);
    memcpy(out->buf, in.buffer, in.length);
    out->size = in.length;
    out->bits_unused = 0;
  }
}

int ngap_ue_context_release_complete(instance_t instance,
                                     ngap_ue_release_complete_t *ue_release_complete_p)
{

  ngap_gNB_instance_t                 *ngap_gNB_instance_p = NULL;
  struct ngap_gNB_ue_context_s        *ue_context_p        = NULL;
  NGAP_NGAP_PDU_t pdu = {0};
  uint8_t  *buffer;
  uint32_t length;

  /* Retrieve the NGAP gNB instance associated with Mod_id */
  ngap_gNB_instance_p = ngap_gNB_get_instance(instance);

  DevAssert(ue_release_complete_p != NULL);
  DevAssert(ngap_gNB_instance_p != NULL);

  if ((ue_context_p = ngap_get_ue_context(ue_release_complete_p->gNB_ue_ngap_id)) == NULL) {
    /* The context for this gNB ue ngap id doesn't exist in the map of gNB UEs */
    NGAP_ERROR("Failed to find ue context associated with gNB ue ngap id: %u\n", ue_release_complete_p->gNB_ue_ngap_id);
    return -1;
  }

  /* Prepare the NGAP message to encode */
  pdu.present = NGAP_NGAP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu.choice.successfulOutcome, head);
  head->procedureCode = NGAP_ProcedureCode_id_UEContextRelease;
  head->criticality = NGAP_Criticality_reject;
  head->value.present = NGAP_SuccessfulOutcome__value_PR_UEContextReleaseComplete;
  NGAP_UEContextReleaseComplete_t *out = &head->value.choice.UEContextReleaseComplete;

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UEContextReleaseComplete_IEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_UEContextReleaseComplete_IEs__value_PR_AMF_UE_NGAP_ID;
    asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID, ue_context_p->amf_ue_ngap_id);
  }

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UEContextReleaseComplete_IEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_UEContextReleaseComplete_IEs__value_PR_RAN_UE_NGAP_ID;
    ie->value.choice.RAN_UE_NGAP_ID = ue_release_complete_p->gNB_ue_ngap_id;
  }

    /* optional */
  {
    // Add PDU Sessions to release
    NGAP_INFO("UE CONTEXT RELEASE COMPLETE for AMF-UE-NGAP-ID:%lu RAN-UE-NGAP-ID:%u Num PDU Ssn: %u\n", ue_context_p->amf_ue_ngap_id, ue_release_complete_p->gNB_ue_ngap_id, ue_release_complete_p->nb_of_pdusessions);
    if (ue_release_complete_p->nb_of_pdusessions > 0) {
      asn1cSequenceAdd(out->protocolIEs.list, NGAP_UEContextReleaseComplete_IEs_t, ie);
      ie->id = NGAP_ProtocolIE_ID_id_PDUSessionResourceListCxtRelCpl;
      ie->criticality = NGAP_Criticality_reject;
      ie->value.present = NGAP_UEContextReleaseComplete_IEs__value_PR_PDUSessionResourceListCxtRelCpl;
      for (int i = 0; i < ue_release_complete_p->nb_of_pdusessions; i++) {
        NGAP_PDUSessionResourceItemCxtRelCpl_t     *item;
        item = (NGAP_PDUSessionResourceItemCxtRelCpl_t *)calloc(1,sizeof(NGAP_PDUSessionResourceItemCxtRelCpl_t));
        item->pDUSessionID = ue_release_complete_p->pdusessions[i].pdusession_id;
        asn1cSeqAdd(&ie->value.choice.PDUSessionResourceListCxtRelCpl.list, item);
      }
    }
  }

  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    /* Encode procedure has failed... */
    NGAP_ERROR("Failed to encode UE context release complete\n");
    return -1;
  }

  /* UE associated signalling -> use the allocated stream */
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance,
                                   ue_context_p->amf_ref->assoc_id, buffer,
                                   length, ue_context_p->tx_stream);

  //LG ngap_gNB_itti_send_sctp_close_association(ngap_gNB_instance_p->instance,
  //                                             ue_context_p->amf_ref->assoc_id);
  // release UE context
  ngap_gNB_ue_context_t *tmp = ngap_detach_ue_context(ue_release_complete_p->gNB_ue_ngap_id);
  if (tmp)
    free(tmp);
  return 0;
}

int ngap_ue_context_release_req(instance_t instance,
                                ngap_ue_release_req_t *ue_release_req_p)
{
  ngap_gNB_instance_t                *ngap_gNB_instance_p           = NULL;
  struct ngap_gNB_ue_context_s       *ue_context_p                  = NULL;
  NGAP_NGAP_PDU_t pdu = {0};
  uint8_t                            *buffer                        = NULL;
  uint32_t                            length;
  /* Retrieve the NGAP gNB instance associated with Mod_id */
  ngap_gNB_instance_p = ngap_gNB_get_instance(instance);

  DevAssert(ue_release_req_p != NULL);
  DevAssert(ngap_gNB_instance_p != NULL);

  if ((ue_context_p = ngap_get_ue_context(ue_release_req_p->gNB_ue_ngap_id)) == NULL) {
    /* The context for this gNB ue ngap id doesn't exist in the map of gNB UEs */
    NGAP_WARN("Failed to find ue context associated with gNB ue ngap id: %u\n",
              ue_release_req_p->gNB_ue_ngap_id);
    /* send response to free the UE: we don't know it, but it should be
     * released since RRC seems to know it (e.g., there is no AMF) */
    MessageDef *msg = itti_alloc_new_message(TASK_NGAP, 0, NGAP_UE_CONTEXT_RELEASE_COMMAND);
    NGAP_UE_CONTEXT_RELEASE_COMMAND(msg).gNB_ue_ngap_id = ue_release_req_p->gNB_ue_ngap_id;
    itti_send_msg_to_task(TASK_RRC_GNB, ngap_gNB_instance_p->instance, msg);
    return -1;
  }

  /* Prepare the NGAP message to encode */
  pdu.present = NGAP_NGAP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu.choice.initiatingMessage, head);
  head->procedureCode = NGAP_ProcedureCode_id_UEContextReleaseRequest;
  head->criticality = NGAP_Criticality_ignore;
  head->value.present = NGAP_InitiatingMessage__value_PR_UEContextReleaseRequest;
  NGAP_UEContextReleaseRequest_t *out = &head->value.choice.UEContextReleaseRequest;

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UEContextReleaseRequest_IEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UEContextReleaseRequest_IEs__value_PR_AMF_UE_NGAP_ID;
    asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID, ue_context_p->amf_ue_ngap_id);
  }

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UEContextReleaseRequest_IEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UEContextReleaseRequest_IEs__value_PR_RAN_UE_NGAP_ID;
    ie->value.choice.RAN_UE_NGAP_ID = ue_release_req_p->gNB_ue_ngap_id;
  }

  /* optional */
  if (ue_release_req_p->nb_of_pdusessions > 0) {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UEContextReleaseRequest_IEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_PDUSessionResourceListCxtRelReq;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_UEContextReleaseRequest_IEs__value_PR_PDUSessionResourceListCxtRelReq;
    for (int i = 0; i < ue_release_req_p->nb_of_pdusessions; i++) {
      NGAP_PDUSessionResourceItemCxtRelReq_t     *item;
      item = (NGAP_PDUSessionResourceItemCxtRelReq_t *)calloc(1,sizeof(NGAP_PDUSessionResourceItemCxtRelReq_t));
      item->pDUSessionID = ue_release_req_p->pdusessions[i].pdusession_id;
      asn1cSeqAdd(&ie->value.choice.PDUSessionResourceListCxtRelReq.list, item);
    }
  }

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_UEContextReleaseRequest_IEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_Cause;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_UEContextReleaseRequest_IEs__value_PR_Cause;
    DevAssert(ue_release_req_p->cause <= NGAP_Cause_PR_choice_ExtensionS);
    switch(ue_release_req_p->cause){
      case NGAP_CAUSE_RADIO_NETWORK:
	ie->value.choice.Cause.present = NGAP_Cause_PR_radioNetwork;
	ie->value.choice.Cause.choice.radioNetwork = ue_release_req_p->cause_value;
	break;
      case NGAP_CAUSE_TRANSPORT:
	ie->value.choice.Cause.present = NGAP_Cause_PR_transport;
	ie->value.choice.Cause.choice.transport = ue_release_req_p->cause_value;
	break;
      case NGAP_CAUSE_NAS:
	ie->value.choice.Cause.present = NGAP_Cause_PR_nas;
	ie->value.choice.Cause.choice.nas = ue_release_req_p->cause_value;
	break;
      case NGAP_CAUSE_PROTOCOL:
	ie->value.choice.Cause.present = NGAP_Cause_PR_protocol;
	ie->value.choice.Cause.choice.protocol = ue_release_req_p->cause_value;
	break;
      case NGAP_CAUSE_MISC:
	ie->value.choice.Cause.present = NGAP_Cause_PR_misc;
	ie->value.choice.Cause.choice.misc = ue_release_req_p->cause_value;
	break;
      default:
        NGAP_WARN("Received NG Error indication cause NGAP_Cause_PR_choice_Extensions\n");
        break;
    }
  }

  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    /* Encode procedure has failed... */
    NGAP_ERROR("Failed to encode UE context release complete\n");
    return -1;
  }

  /* UE associated signalling -> use the allocated stream */
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance,
                                   ue_context_p->amf_ref->assoc_id, buffer,
                                   length, ue_context_p->tx_stream);

  return 0;
}
int ngap_gNB_handover_request_acknowledge(instance_t instance, ngap_handover_request_ack_t* handover_req_ack_p)
{
  NGAP_INFO("Preparing HO Request Acknowledge Message!\n");
  ngap_gNB_instance_t            *ngap_gNB_instance_p = NULL;
  struct ngap_gNB_ue_context_s   *ue_context_p        = NULL;
  NGAP_NGAP_PDU_t pdu;
  uint8_t  *buffer  = NULL;
  uint32_t length;
  /* Retrieve the NGAP gNB instance associated with Mod_id */
  ngap_gNB_instance_p = ngap_gNB_get_instance(instance);
  DevAssert(handover_req_ack_p != NULL);
  DevAssert(ngap_gNB_instance_p != NULL);

  if ((ue_context_p = ngap_get_ue_context(handover_req_ack_p->gNB_ue_ngap_id)) == NULL) {
    /* The context for this gNB ue ngap id doesn't exist in the map of gNB UEs */
    NGAP_WARN("Failed to find ue context associated with gNB ue ngap id: 0x%08x\n", handover_req_ack_p->gNB_ue_ngap_id);
    return -1;
  }  
  /* Prepare the NGAP message to encode */
  memset(&pdu, 0, sizeof(pdu));
  pdu.present = NGAP_NGAP_PDU_PR_successfulOutcome;
  asn1cCalloc(pdu.choice.successfulOutcome, head);
  head->procedureCode = NGAP_ProcedureCode_id_HandoverResourceAllocation;
  head->criticality = NGAP_Criticality_reject;
  head->value.present = NGAP_SuccessfulOutcome__value_PR_HandoverRequestAcknowledge;
  NGAP_HandoverRequestAcknowledge_t *out = &head->value.choice.HandoverRequestAcknowledge; 

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequestAcknowledgeIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_HandoverRequestAcknowledgeIEs__value_PR_AMF_UE_NGAP_ID;
    asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID, handover_req_ack_p->amf_ue_ngap_id);
  }
    /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequestAcknowledgeIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_HandoverRequestAcknowledgeIEs__value_PR_RAN_UE_NGAP_ID;
    ie->value.choice.RAN_UE_NGAP_ID = (int64_t)handover_req_ack_p->gNB_ue_ngap_id;
  }

    /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequestAcknowledgeIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_TargetToSource_TransparentContainer;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverRequestAcknowledgeIEs__value_PR_TargetToSource_TransparentContainer;
    

    NGAP_TargetNGRANNode_ToSourceNGRANNode_TransparentContainer_t* transparentContainer =  (NGAP_TargetNGRANNode_ToSourceNGRANNode_TransparentContainer_t*)calloc(1, sizeof(NGAP_TargetNGRANNode_ToSourceNGRANNode_TransparentContainer_t));
    //rRC Container
    {
      // Handover Command
      transparentContainer->rRCContainer.size = handover_req_ack_p->targetToSourceTransparentContainer.length;
      transparentContainer->rRCContainer.buf =  (uint8_t *)malloc(handover_req_ack_p->targetToSourceTransparentContainer.length);
      memcpy(transparentContainer->rRCContainer.buf, handover_req_ack_p->targetToSourceTransparentContainer.buffer ,handover_req_ack_p->targetToSourceTransparentContainer.length);
      uint8_t *buf = NULL;
      int encoded = aper_encode_to_new_buffer(&asn_DEF_NGAP_TargetNGRANNode_ToSourceNGRANNode_TransparentContainer, NULL, transparentContainer, (void**)&buf);
      if (encoded < 0){
        LOG_E(NR_RRC, "HO LOG: asn_DEF_NGAP_TargetNGRANNode_ToSourceNGRANNode_TransparentContainer ASN1 message encoding failed %d!\n\n", encoded);
        return -1;
      }
      int ret = OCTET_STRING_fromBuf(&ie->value.choice.TargetToSource_TransparentContainer, (const char *)buf, encoded);
      if (ret != 0) {
        LOG_E(NR_RRC, "HO LOG: Can not perform OCTET_STRING_fromBuf for the TargetToSource_TransparentContainer");
        return -1;
      }
    }
  }

      /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequestAcknowledgeIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_PDUSessionResourceAdmittedList;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_HandoverRequestAcknowledgeIEs__value_PR_PDUSessionResourceAdmittedList;

    for (int pduSesIdx = 0; pduSesIdx < handover_req_ack_p->nb_of_pdusessions; pduSesIdx++)
    {
      asn1cSequenceAdd(ie->value.choice.PDUSessionResourceAdmittedList.list, NGAP_PDUSessionResourceAdmittedItem_t, hoPduSesAdmittedItem);
      hoPduSesAdmittedItem->pDUSessionID = handover_req_ack_p->pdusessions[pduSesIdx].pdusession_id;
      /* dLQosFlowPerTNLInformation */
      NGAP_HandoverRequestAcknowledgeTransfer_t hoReqTransfer = {0};
      hoReqTransfer.dL_NGU_UP_TNLInformation.present = NGAP_UPTransportLayerInformation_PR_gTPTunnel;
      asn1cCalloc(hoReqTransfer.dL_NGU_UP_TNLInformation.choice.gTPTunnel, tmp);
      GTP_TEID_TO_ASN1(handover_req_ack_p->pdusessions[pduSesIdx].gtp_teid, &tmp->gTP_TEID);
      allocAddrCopy(&tmp->transportLayerAddress, handover_req_ack_p->pdusessions[pduSesIdx].gNB_addr);

      for (int j = 0; j < handover_req_ack_p->pdusessions[pduSesIdx].nb_of_qos_flow; j++)
      {
        asn1cSequenceAdd(hoReqTransfer.qosFlowSetupResponseList.list, NGAP_QosFlowItemWithDataForwarding_t, qosItem);
        qosItem->qosFlowIdentifier = handover_req_ack_p->pdusessions[pduSesIdx].associated_qos_flows[j].qfi;
        qosItem->dataForwardingAccepted = NGAP_DataForwardingAccepted_data_forwarding_accepted;
      }

      void *hoReqTransfer_buffer;
      ssize_t encoded = aper_encode_to_new_buffer(&asn_DEF_NGAP_HandoverRequestAcknowledgeTransfer, NULL, &hoReqTransfer, &hoReqTransfer_buffer);           
      if (encoded < 0){
        LOG_E(NR_RRC, "HO LOG: asn_DEF_NGAP_HandoverRequestAcknowledgeTransfer ASN1 message encoding failed %ld!\n\n", encoded);
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NGAP_HandoverRequestAcknowledgeTransfer, &hoReqTransfer);
        ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NGAP_NGAP_PDU, &pdu);
        return -1;
      }
      hoPduSesAdmittedItem->handoverRequestAcknowledgeTransfer.buf = hoReqTransfer_buffer;
      hoPduSesAdmittedItem->handoverRequestAcknowledgeTransfer.size = encoded;

      ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NGAP_HandoverRequestAcknowledgeTransfer, &hoReqTransfer);
    }
  }

  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_NGAP_NGAP_PDU, (void *)&pdu);

  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    /* Encode procedure has failed... */
    NGAP_ERROR("Failed to encode Handover Request Acknowledge Msg\n");
    return -1;
  }

  /* UE associated signalling -> use the allocated stream */
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance,
                                   ue_context_p->amf_ref->assoc_id, buffer,
                                   length, ue_context_p->tx_stream);

  return 0;
}

int ngap_gNB_handover_notify(instance_t instance, ngap_handover_notify_t* handover_notify_p)
{
  LOG_I(NR_RRC, "HO LOG: NGAP Handover Required Preparation!\n");
  ngap_gNB_instance_t            *ngap_gNB_instance_p = NULL;
  struct ngap_gNB_ue_context_s   *ue_context_p        = NULL;
  NGAP_NGAP_PDU_t pdu;
  uint8_t  *buffer  = NULL;
  uint32_t length;
  /* Retrieve the NGAP gNB instance associated with Mod_id */
  ngap_gNB_instance_p = ngap_gNB_get_instance(instance);
  DevAssert(handover_notify_p != NULL);
  DevAssert(ngap_gNB_instance_p != NULL);

  if ((ue_context_p = ngap_get_ue_context(handover_notify_p->gNB_ue_ngap_id)) == NULL) {
    /* The context for this gNB ue ngap id doesn't exist in the map of gNB UEs */
    NGAP_WARN("Failed to find ue context associated with gNB ue ngap id: 0x%08x\n", handover_notify_p->gNB_ue_ngap_id);
    return -1;
  }  

  /* Prepare the NGAP message to encode */
  memset(&pdu, 0, sizeof(pdu));
  pdu.present = NGAP_NGAP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu.choice.initiatingMessage, head);
  head->procedureCode = NGAP_ProcedureCode_id_HandoverNotification;
  head->criticality = NGAP_Criticality_ignore;
  head->value.present = NGAP_InitiatingMessage__value_PR_HandoverNotify;
  NGAP_HandoverNotify_t *out = &head->value.choice.HandoverNotify;  

    /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverNotifyIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverNotifyIEs__value_PR_AMF_UE_NGAP_ID;
    asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID, ue_context_p->amf_ue_ngap_id);
  }


    /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverNotifyIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverNotifyIEs__value_PR_RAN_UE_NGAP_ID;
    ie->value.choice.RAN_UE_NGAP_ID = (int64_t)ue_context_p->gNB_ue_ngap_id;
  }

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverNotifyIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_UserLocationInformation;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_HandoverNotifyIEs__value_PR_UserLocationInformation;

    ie->value.choice.UserLocationInformation.present = NGAP_UserLocationInformation_PR_userLocationInformationNR;

    asn1cCalloc(ie->value.choice.UserLocationInformation.choice.userLocationInformationNR, userinfo_nr_p);

    //TODO - Firecell: fix NRCGI!
    /* Set nRCellIdentity. default userLocationInformationNR */
    // NR_CELL_ID_TO_BIT_STRING(handover_notify_p->nrCellId, &userinfo_nr_p->nR_CGI.nRCellIdentity);
    MACRO_GNB_ID_TO_CELL_IDENTITY(ngap_gNB_instance_p->gNB_id,
                                  handover_notify_p->nrCellId, // Cell ID
                                  &userinfo_nr_p->nR_CGI.nRCellIdentity);

    MCC_MNC_TO_TBCD(ngap_gNB_instance_p->plmn[ue_context_p->selected_plmn_identity].mcc,
                    ngap_gNB_instance_p->plmn[ue_context_p->selected_plmn_identity].mnc,
                    ngap_gNB_instance_p->plmn[ue_context_p->selected_plmn_identity].mnc_digit_length,
                    &userinfo_nr_p->nR_CGI.pLMNIdentity);

    // /* Set TAI */
    INT24_TO_OCTET_STRING(ngap_gNB_instance_p->tac, &userinfo_nr_p->tAI.tAC);
    MCC_MNC_TO_PLMNID(ngap_gNB_instance_p->plmn[ue_context_p->selected_plmn_identity].mcc,
                      ngap_gNB_instance_p->plmn[ue_context_p->selected_plmn_identity].mnc,
                      ngap_gNB_instance_p->plmn[ue_context_p->selected_plmn_identity].mnc_digit_length,
                      &userinfo_nr_p->tAI.pLMNIdentity);
  }

  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    /* Encode procedure has failed... */
    NGAP_ERROR("Failed to encode Handover Required Msg\n");
    return -1;
  }

  /* UE associated signalling -> use the allocated stream */
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance,
                                   ue_context_p->amf_ref->assoc_id, buffer,
                                   length, ue_context_p->tx_stream);

  return 0;
}

int ngap_gNB_handover_required(instance_t instance, ngap_handover_required_t* handover_required_p)
{
  LOG_I(NR_RRC, "HO LOG: NGAP Handover Required Preparation!\n");
  ngap_gNB_instance_t            *ngap_gNB_instance_p = NULL;
  struct ngap_gNB_ue_context_s   *ue_context_p        = NULL;
  NGAP_NGAP_PDU_t pdu;
  uint8_t  *buffer  = NULL;
  uint32_t length;
  /* Retrieve the NGAP gNB instance associated with Mod_id */
  ngap_gNB_instance_p = ngap_gNB_get_instance(instance);
  DevAssert(handover_required_p != NULL);
  DevAssert(ngap_gNB_instance_p != NULL);

  if ((ue_context_p = ngap_get_ue_context(handover_required_p->gNB_ue_ngap_id)) == NULL) {
    /* The context for this gNB ue ngap id doesn't exist in the map of gNB UEs */
    NGAP_WARN("Failed to find ue context associated with gNB ue ngap id: 0x%08x\n", handover_required_p->gNB_ue_ngap_id);
    return -1;
  }  

  /* Prepare the NGAP message to encode */
  memset(&pdu, 0, sizeof(pdu));
  pdu.present = NGAP_NGAP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu.choice.initiatingMessage, head);
  head->procedureCode = NGAP_ProcedureCode_id_HandoverPreparation;
  head->criticality = NGAP_Criticality_reject;
  head->value.present = NGAP_InitiatingMessage__value_PR_HandoverRequired;
  NGAP_HandoverRequired_t *out = &head->value.choice.HandoverRequired;

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequiredIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverRequiredIEs__value_PR_AMF_UE_NGAP_ID;
    asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID, ue_context_p->amf_ue_ngap_id);
  }


    /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequiredIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverRequiredIEs__value_PR_RAN_UE_NGAP_ID;
    ie->value.choice.RAN_UE_NGAP_ID = (int64_t)ue_context_p->gNB_ue_ngap_id;
  }

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequiredIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_HandoverType;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverRequiredIEs__value_PR_HandoverType;
    ie->value.choice.HandoverType = (int64_t)handover_required_p->handoverType; //NGAP_HandoverType_intra5gs
  }

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequiredIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_Cause;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_HandoverRequiredIEs__value_PR_Cause;
    ie->value.choice.Cause.present = NGAP_Cause_PR_radioNetwork;
    ie->value.choice.Cause.choice.radioNetwork = handover_required_p->cause; //NGAP_CauseRadioNetwork_handover_desirable_for_radio_reason
  }

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequiredIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_TargetID;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverRequiredIEs__value_PR_TargetID;
    ie->value.choice.TargetID.present = NGAP_TargetID_PR_targetRANNodeID;
    asn1cCalloc(ie->value.choice.TargetID.choice.targetRANNodeID, targetRan);
    targetRan->globalRANNodeID.present = NGAP_GlobalRANNodeID_PR_globalGNB_ID;
    asn1cCalloc(targetRan->globalRANNodeID.choice.globalGNB_ID, globalGnbId);

    globalGnbId->gNB_ID.present = NGAP_GNB_ID_PR_gNB_ID;
    
    MACRO_GNB_ID_TO_BIT_STRING(handover_required_p->target_gnb_id.targetgNBId,
                                &globalGnbId->gNB_ID.choice.gNB_ID);


    MCC_MNC_TO_PLMNID(handover_required_p->target_gnb_id.plmn_identity.mcc,
                    handover_required_p->target_gnb_id.plmn_identity.mnc,
                    handover_required_p->target_gnb_id.plmn_identity.mnc_digit_length,
                    &globalGnbId->pLMNIdentity);

    /* Set TAI */
    INT24_TO_OCTET_STRING(handover_required_p->target_gnb_id.tac, &targetRan->selectedTAI.tAC);
    MCC_MNC_TO_PLMNID(handover_required_p->target_gnb_id.plmn_identity.mcc,
                    handover_required_p->target_gnb_id.plmn_identity.mnc,
                    handover_required_p->target_gnb_id.plmn_identity.mnc_digit_length,
                    &targetRan->selectedTAI.pLMNIdentity);

  }

    /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequiredIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_PDUSessionResourceListHORqd;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverRequiredIEs__value_PR_PDUSessionResourceListHORqd;
    for (int i = 0; i < handover_required_p->nb_of_pdusessions; ++i) {
      asn1cSequenceAdd(ie->value.choice.PDUSessionResourceListHORqd.list, NGAP_PDUSessionResourceItemHORqd_t, hoRequiredPduSession);
      hoRequiredPduSession->pDUSessionID = handover_required_p->pdusessions[i].pdusession_id;
      
      NGAP_HandoverRequiredTransfer_t hoRequiredTransfer = {0};
      asn1cCalloc(hoRequiredTransfer.directForwardingPathAvailability, directFwdPathAvl);
      *directFwdPathAvl = NGAP_DirectForwardingPathAvailability_direct_path_available; //set False! (?)

      // void *hoRequiredTransferPduSessionBuf;

      asn_enc_rval_t enc_rval;
      uint8_t ho_req_transfer_transparent_container_buffer[128] = {0};
      xer_fprint(stdout, &asn_DEF_NGAP_HandoverRequiredTransfer, &hoRequiredTransfer);
      enc_rval = aper_encode_to_buffer(&asn_DEF_NGAP_HandoverRequiredTransfer,
                                       NULL,
                                       &hoRequiredTransfer,
                                       ho_req_transfer_transparent_container_buffer,
                                       128);

      AssertFatal(enc_rval.encoded > 0, "ASN1 message encoding failed (%s, %lu)!\n", enc_rval.failed_type->name, enc_rval.encoded);

      hoRequiredPduSession->handoverRequiredTransfer.buf = CALLOC(1, (enc_rval.encoded + 7) / 8);
      memcpy(hoRequiredPduSession->handoverRequiredTransfer.buf,
             ho_req_transfer_transparent_container_buffer,
             (enc_rval.encoded + 7) / 8);
      hoRequiredPduSession->handoverRequiredTransfer.size = (enc_rval.encoded + 7) / 8;

      ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NGAP_HandoverRequiredTransfer, &hoRequiredTransfer);
    }
  }

  /* optional */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequiredIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_DirectForwardingPathAvailability;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_HandoverRequiredIEs__value_PR_DirectForwardingPathAvailability;
    ie->value.choice.DirectForwardingPathAvailability = NGAP_DirectForwardingPathAvailability_direct_path_available;
  }

  /* mandatory */
  {
    asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverRequiredIEs_t, ie);
    ie->id = NGAP_ProtocolIE_ID_id_SourceToTarget_TransparentContainer;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_HandoverRequiredIEs__value_PR_SourceToTarget_TransparentContainer;
    

    NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer_t* transparentContainer =  (NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer_t*)calloc(1, sizeof(NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer_t));
    
    //rRC Container
    {
      // Handover Prep Information
      transparentContainer->rRCContainer.size = handover_required_p->sourceToTargetContainer->handoverInfo.length;
      transparentContainer->rRCContainer.buf =  (uint8_t *)malloc(handover_required_p->sourceToTargetContainer->handoverInfo.length);
      memcpy(transparentContainer->rRCContainer.buf, handover_required_p->sourceToTargetContainer->handoverInfo.buffer ,transparentContainer->rRCContainer.size);
      
    }


    //PDU Session Information
    {
      asn1cCalloc(transparentContainer->pDUSessionResourceInformationList, pduSessionList);
      for (uint8_t i = 0; i < handover_required_p->nb_of_pdusessions; ++i)
      {
        const pdusession_setup_t *pduSession = &handover_required_p->pdusessions[i];
        LOG_I(NR_RRC, "HO LOG: Preparing NGAP_PDUSessionResourceInformationItem_t . pdusesId: %d\n", pduSession->pdusession_id);

        asn1cSequenceAdd(pduSessionList->list, NGAP_PDUSessionResourceInformationItem_t, pduSesResourceStp);
        pduSesResourceStp->pDUSessionID = (uint64_t)pduSession->pdusession_id;

        LOG_E(NR_RRC, "HO LOG: Number of QoS Flows : %d\n", pduSession->nb_of_qos_flow);
        for (int qosFlowId = 0; qosFlowId < pduSession->nb_of_qos_flow; ++qosFlowId) {
          asn1cSequenceAdd(pduSesResourceStp->qosFlowInformationList.list, NGAP_QosFlowInformationItem_t, qosFlowInfo);
          const pdusession_associate_qosflow_t *qf = &pduSession->associated_qos_flows[qosFlowId];
          LOG_I(NR_RRC, "HO LOG: Processing QFI : %d\n", qf->qfi);
          qosFlowInfo->qosFlowIdentifier = (uint64_t)qf->qfi;
        }
      }
    }

    // NrCGI - Mandatory
    {
      transparentContainer->targetCell_ID.present = NGAP_NGRAN_CGI_PR_nR_CGI;
      asn1cCalloc(transparentContainer->targetCell_ID.choice.nR_CGI, tNrCGI);
      tNrCGI->iE_Extensions = NULL;
      MCC_MNC_TO_PLMNID(handover_required_p->target_gnb_id.plmn_identity.mcc,
                      handover_required_p->target_gnb_id.plmn_identity.mnc,
                      handover_required_p->target_gnb_id.plmn_identity.mnc_digit_length,
                      &tNrCGI->pLMNIdentity);

      NR_CELL_ID_TO_BIT_STRING(handover_required_p->sourceToTargetContainer->targetCellId.nrCellIdentity, &tNrCGI->nRCellIdentity);
    }

    //UE history Information - Mandatory
    {
      asn1cSequenceAdd(transparentContainer->uEHistoryInformation.list, NGAP_LastVisitedCellItem_t, lastVisitedCell);
      lastVisitedCell->iE_Extensions = NULL;
      lastVisitedCell->lastVisitedCellInformation.present = NGAP_LastVisitedCellInformation_PR_nGRANCell;
      asn1cCalloc(lastVisitedCell->lastVisitedCellInformation.choice.nGRANCell, lastVisitedNR);
      lastVisitedNR->cellType.cellSize = NGAP_CellSize_small;
      lastVisitedNR->globalCellID.present = NGAP_NGRAN_CGI_PR_nR_CGI;

      asn1cCalloc(lastVisitedNR->globalCellID.choice.nR_CGI, lastVisitedNrCGI);
      MCC_MNC_TO_PLMNID(handover_required_p->target_gnb_id.plmn_identity.mcc,
                      handover_required_p->target_gnb_id.plmn_identity.mnc,
                      handover_required_p->target_gnb_id.plmn_identity.mnc_digit_length,
                      &lastVisitedNrCGI->pLMNIdentity);

      NR_CELL_ID_TO_BIT_STRING(handover_required_p->sourceToTargetContainer->targetCellId.nrCellIdentity, &lastVisitedNrCGI->nRCellIdentity);

      asn1cCalloc(lastVisitedNR->hOCauseValue, lastVisitedCause);
      lastVisitedCause->present = NGAP_Cause_PR_radioNetwork;
      lastVisitedCause->choice.radioNetwork = NGAP_CauseRadioNetwork_handover_desirable_for_radio_reason;
      //TODO: It is mandatory adding dummy number for now
      lastVisitedNR->timeUEStayedInCell = (int64_t)500;
    }
    if (LOG_DEBUGFLAG(DEBUG_ASN1))
      xer_fprint(stdout, &asn_DEF_NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer, transparentContainer);

    uint8_t source_to_target_transparent_container_buf[16384] = {0};
    asn_enc_rval_t enc_rval;
    enc_rval = aper_encode_to_buffer(&asn_DEF_NGAP_SourceNGRANNode_ToTargetNGRANNode_TransparentContainer,
                                     NULL,
                                     (void *)transparentContainer,
                                     (void *)&source_to_target_transparent_container_buf,
                                     16384);
    if (enc_rval.encoded < 0) {
      AssertFatal(enc_rval.encoded > 0,
                  "HO LOG: Source to Transparent ASN1 message encoding failed (%s, %lu)!\n",
                  enc_rval.failed_type->name,
                  enc_rval.encoded);
      return -1;
    }
    LOG_I(NR_RRC, "HO LOG Encoding Result and Length of The Transparent Container: %lu!\n\n", enc_rval.encoded);
    int total_bytes = (enc_rval.encoded + 7) / 8;
    int ret = OCTET_STRING_fromBuf(&ie->value.choice.SourceToTarget_TransparentContainer,
                                   (const char *)&source_to_target_transparent_container_buf,
                                   total_bytes);
    if (ret != 0) {
      LOG_E(NR_RRC, "HO LOG: Can not perform OCTET_STRING_fromBuf for the SourceToTarget_TransparentContainer");
      return -1;
    }
  }
  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_NGAP_NGAP_PDU, &pdu);

  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    /* Encode procedure has failed... */
    NGAP_ERROR("Failed to encode Handover Required Msg\n");
    return -1;
  }

  /* UE associated signalling -> use the allocated stream */
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance,
                                   ue_context_p->amf_ref->assoc_id, buffer,
                                   length, ue_context_p->tx_stream);

  return 0;
}

int ngap_gNB_handover_cancel(instance_t instance, ngap_handover_cancel_t* handover_cancel_p)
{
 
    LOG_I(NR_RRC, "HO LOG: NGAP Handover Cancel Trigger\n");
    ngap_gNB_instance_t            *ngap_gNB_instance_p = NULL;
    struct ngap_gNB_ue_context_s   *ue_context_p        = NULL;
    NGAP_NGAP_PDU_t pdu;
    uint8_t  *buffer  = NULL;
    uint32_t length;
    /* Retrieve the NGAP gNB instance associated with Mod_id */
    ngap_gNB_instance_p = ngap_gNB_get_instance(instance);
    DevAssert(handover_cancel_p != NULL);
    DevAssert(ngap_gNB_instance_p != NULL);

    if ((ue_context_p = ngap_get_ue_context(handover_cancel_p->gNB_ue_ngap_id)) == NULL) {
       /* The context for this gNB ue ngap id doesn't exist in the map of gNB UEs */
       NGAP_WARN("Failed to find ue context associated with gNB ue ngap id: 0x%08x\n", handover_cancel_p->gNB_ue_ngap_id);
       return -1;
    }

    /* Prepare the NGAP message to encode */
    memset(&pdu, 0, sizeof(pdu));
    pdu.present = NGAP_NGAP_PDU_PR_initiatingMessage;
    asn1cCalloc(pdu.choice.initiatingMessage, head);
    head->procedureCode = NGAP_ProcedureCode_id_HandoverCancel;
    head->criticality = NGAP_Criticality_reject;
    head->value.present = NGAP_InitiatingMessage__value_PR_HandoverCancel;
    NGAP_HandoverCancel_t *out = &head->value.choice.HandoverCancel;

    /* mandatory */
    {
       asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverCancelIEs_t, ie);
       ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
       ie->criticality = NGAP_Criticality_reject;
       ie->value.present = NGAP_HandoverCancelIEs__value_PR_AMF_UE_NGAP_ID;
       asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID,handover_cancel_p->amf_ue_ngap_id);
    } 

    /* mandatory */
    {
       asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverCancelIEs_t, ie);
       ie->id = NGAP_ProtocolIE_ID_id_RAN_UE_NGAP_ID;
       ie->criticality = NGAP_Criticality_reject;
       ie->value.present = NGAP_HandoverCancelIEs__value_PR_RAN_UE_NGAP_ID;
       ie->value.choice.RAN_UE_NGAP_ID = (int64_t)handover_cancel_p->gNB_ue_ngap_id;
    }
    
    /* mandatory */
    {
        asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverCancelIEs_t, ie);
        ie->id = NGAP_ProtocolIE_ID_id_Cause;
        ie->criticality = NGAP_Criticality_ignore;
        ie->value.present = NGAP_HandoverCancelIEs__value_PR_Cause;
        ie->value.choice.Cause.present = NGAP_Cause_PR_radioNetwork;
        ie->value.choice.Cause.choice.radioNetwork = handover_cancel_p->cause; //NGAP_CauseRadioNetwork_handover_desirable_for_radio_reason
    }
  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    /* Encode procedure has failed... */
    NGAP_ERROR("Failed to encode UE context release complete\n");
    return -1;
  }

  /* UE associated signalling -> use the allocated stream */
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance,
                                   ue_context_p->amf_ref->assoc_id, buffer,
                                   length, ue_context_p->tx_stream);
  return 0;

}

int ngap_gNB_handover_failure(instance_t instance, ngap_handover_failure_t* handover_failure_p)
{

    LOG_I(NR_RRC, "HO LOG: NGAP Handover failure Trigger\n");
    ngap_gNB_instance_t            *ngap_gNB_instance_p = NULL;
    struct ngap_gNB_ue_context_s   *ue_context_p        = NULL;
    NGAP_NGAP_PDU_t pdu;
    uint8_t  *buffer  = NULL;
    uint32_t length;
    /* Retrieve the NGAP gNB instance associated with Mod_id */
    ngap_gNB_instance_p = ngap_gNB_get_instance(instance);
    DevAssert(handover_failure_p != NULL);
    DevAssert(ngap_gNB_instance_p != NULL);
    ngap_gNB_amf_data_t *ngap_amf_data_p = NULL;
    ngap_amf_data_p = ngap_gNB_get_AMF_from_instance(ngap_gNB_instance_p);

    if(handover_failure_p->gNB_ue_ngap_id != MAX_UINT32_RANGE){
       if ((ue_context_p = ngap_get_ue_context(handover_failure_p->gNB_ue_ngap_id)) == NULL) {
        //The context for this gNB ue ngap id doesn't exist in the map of gNB UEs
       NGAP_WARN("Failed to find ue context associated with gNB ue ngap id: 0x%08x\n", handover_failure_p->gNB_ue_ngap_id);
       return -1;
       }
    }

    /* Prepare the NGAP message to encode */
    memset(&pdu, 0, sizeof(pdu));
    pdu.present = NGAP_NGAP_PDU_PR_unsuccessfulOutcome;
    asn1cCalloc(pdu.choice.unsuccessfulOutcome, head);
    head->procedureCode = NGAP_ProcedureCode_id_HandoverResourceAllocation;
    head->criticality = NGAP_Criticality_reject;
    head->value.present = NGAP_UnsuccessfulOutcome__value_PR_HandoverFailure;
    NGAP_HandoverFailure_t *out = &head->value.choice.HandoverFailure;

    /* mandatory */
    {
       asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverFailureIEs_t, ie);
       ie->id = NGAP_ProtocolIE_ID_id_AMF_UE_NGAP_ID;
       ie->criticality = NGAP_Criticality_reject;
       ie->value.present = NGAP_HandoverFailureIEs__value_PR_AMF_UE_NGAP_ID;
       asn_uint642INTEGER(&ie->value.choice.AMF_UE_NGAP_ID,handover_failure_p->amf_ue_ngap_id);
    }

    /* mandatory */
    {
        asn1cSequenceAdd(out->protocolIEs.list, NGAP_HandoverFailureIEs_t, ie);
        ie->id = NGAP_ProtocolIE_ID_id_Cause;
        ie->criticality = NGAP_Criticality_ignore;
        ie->value.present = NGAP_HandoverFailureIEs__value_PR_Cause;
        ie->value.choice.Cause.present = NGAP_Cause_PR_radioNetwork;
        ie->value.choice.Cause.choice.radioNetwork = handover_failure_p->cause;
    }
  if (ngap_gNB_encode_pdu(&pdu, &buffer, &length) < 0) {
    /* Encode procedure has failed... */
    NGAP_ERROR("Failed to encode HANDOVER FAILURE MESSAGE\n");
    return -1;
  }
  if(ue_context_p != NULL){
 //  UE associated signalling -> use the allocated stream
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance,
                                   ue_context_p->amf_ref->assoc_id, buffer,
                                   length, ue_context_p->tx_stream);
  return 0;
  }
  /*In this case there is no ue context since HO REQ might be failed at preprocessing stage itself.
   * so sctp message send to AMF based on its assoc_id  */
  ngap_gNB_itti_send_sctp_data_req(ngap_gNB_instance_p->instance, ngap_amf_data_p->assoc_id, buffer, length, 0);
  return 0;

}
