/** TODO: We shall probably move the contents of this file to any existing file
 * it will help us to remove the modifications in the existing file also.
 */

#ifndef NR_UE_RELEASE_DEFS_H
#define NR_UE_RELEASE_DEFS_H

#include "common/utils/assertions.h"
#include "RRC/NR/nr_rrc_defs.h"
#include "RRC/NR/nr_rrc_config.h"
#include "RRC/NR/rrc_gNB_UE_context.h"
#include "COLLECTOR/du_collector.h"

#define RELEASE_UE_CU(a,b,c,d) rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(a,b,c,d)

#define RELEASE_UE_DU(a, b)                                                              \
  do {                                                                                   \
    fprintf(stdout, "DU UE Release \n");                                                 \
    fflush(stdout);                                                                      \
    gNB_MAC_INST* nrmac = RC.nrmac[b];                                                   \
    if (!du_exists_f1_ue_data(a)) {                                                      \
      fprintf(stderr, "UE F1AP Context %d does not exist. Releasing from DU only\n", a); \
      fflush(stdout);                                                                    \
      NR_UE_info_t* ue = (NR_UE_info_t*)get_nr_mac_ue_by_rnti(a);                        \
      if (ue) {                                                                          \
        nr_mac_trigger_release_timer(&ue->UE_sched_ctrl, 1);                             \
      }                                                                                  \
      return;                                                                            \
    }                                                                                    \
    f1_ue_data_t ue_data = du_get_f1_ue_data(a);                                         \
    f1ap_ue_context_release_req_t request = {                                            \
        .gNB_CU_ue_id = ue_data.secondary_ue,                                            \
        .gNB_DU_ue_id = a,                                                               \
        .cause = F1AP_CAUSE_RADIO_NETWORK,                                               \
        .cause_value = F1AP_CauseRadioNetwork_unspecified,                               \
    };                                                                                   \
    nrmac->mac_rrc.ue_context_release_request(&request);                                 \
  } while (0);

#define AssertFatalUeRet(cOND, iNSTiD, fORMAT, aRGS...)		do { 		\
	if (!(cOND)) {								\
	  fprintf(stderr, "\nAssertion (%s) failed!\n"   			\
                "In %s() %s:%d\n" fORMAT,                   			\
                #cOND, __FUNCTION__, __FILE__, __LINE__, ##aRGS);  		\
	  if (RC.nrrrc[iNSTiD]->do_release_on_assert)	{			\
	  fprintf(stdout, "AssertFatalUeRet \n"); \
	  fflush(stdout); \
		return 0;  							\
	  } else {		  						\
		_Assert_Exit_;							\
	  }									\
	} 			     						\
     } while (0);


#define AssertFatalUeDu(cOND, rNTI, mID, fORMAT, aRGS...)		do { 		\
	if (!(cOND)) {								\
	  if (RC.nrmac[mID]->radio_config.do_release_on_assert)	{		\
		RELEASE_UE_DU(rNTI, mID); \
		fprintf(stdout, "AssertFatalUeDu \n"); \
		fflush(stdout); \
		return; 						\
	  } else {		  						\
		fprintf(stderr, "\nAssertion (%s) failed!\n"   			\
                "In %s() %s:%d\n" fORMAT,                   			\
                #cOND, __FUNCTION__, __FILE__, __LINE__, ##aRGS);  		\
		_Assert_Exit_;							\
	  }									\
	} 			     						\
     } while (0);

#define AssertFatalUeCu(cOND, iNSTiD, cTXT, fORMAT, aRGS...)	do { 		\
	if (!(cOND)) {								\
	  if (RC.nrrrc[iNSTiD]->do_release_on_assert)	{			\
		RELEASE_UE_CU(iNSTiD, cTXT,                     		\
		  NGAP_CAUSE_RADIO_NETWORK, NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_NGRAN_GENERATED_REASON);  \
		   fprintf(stdout, "AssertFatalUeCu \n"); \
		   fflush(stdout); \
		  return -1; \
	  } else {		  						\
		fprintf(stderr, "\nAssertion (%s) failed!\n"   			\
                "In %s() %s:%d\n" fORMAT,                   			\
                #cOND, __FUNCTION__, __FILE__, __LINE__, ##aRGS);  		\
		_Assert_Exit_;							\
	  }									\
	} 			     						\
     } while (0);

#endif /** NR_UE_RELEASE_DEFS_H */
