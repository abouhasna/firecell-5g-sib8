#ifndef DU_COLLECTOR_H_
#define DU_COLLECTOR_H_

#include "collector_defs.h"

#define DU_Collector_UE_Iterator(BaSe, VaR) du_collector_ue_stats_t ** VaR##pptr=BaSe, *VaR; while ((VaR=*(VaR##pptr++)))
#define DU_Collector_Cell_Iterator(BaSe, VaR) du_collector_cell_stats_t ** VaR##pptr=BaSe, *VaR; while ((VaR=*(VaR##pptr++)))

extern du_collector_struct_t du_collector_struct_current;
extern du_collector_struct_t du_collector_struct_previous;
void *NR_DU_Collector_Task(void *arg);

extern void (*nr_collector_trigger)(int, int, int);

void du_release_all_ues(void);
void *get_nr_mac_ue_by_rnti(uint16_t rnti);
void nr_collector_trigger_func(int CC_id, int frame, int subframe);
void nr_collector_trigger_fake(int CC_id, int frame, int subframe);

#endif 