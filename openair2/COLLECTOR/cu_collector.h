#ifndef CU_COLLECTOR_H_
#define CU_COLLECTOR_H_

#include "collector_defs.h"
#include "collector_rest_manager.h"


void *NR_CU_Collector_Task(void *arg);

void set_cu_collector_cell_state_inactive(uint64_t nr_cell_id);

uint8_t get_cu_collector_cell_number(void);
uint8_t get_cu_collector_number_of_ues(void);
void reset_cu_ue_info_list(void);

#endif 