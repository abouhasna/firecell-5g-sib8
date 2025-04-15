#ifndef COLLECTOR_DB_MANAGER_H_
#define COLLECTOR_DB_MANAGER_H_

#include <stdio.h>
#include <sqlite3.h>
#include <hiredis/hiredis.h>

struct du_collector_struct;
struct du_collector_ue_stats;
struct du_collector_cell_stats;

// MYSQL Functions
// void create_connector_database(sqlite3** db);
// calculate delta value before writing to database.
// int insert_ue_object(struct du_collector_ue_stats* ue, sqlite3* db);
// int insert_nr_mac_connect_struct(struct du_collector_struct* collector_struct, sqlite3** db);

// REDIS Functions
int connect_redis_database(redisContext** redis, const char* host, uint16_t port);
int redis_database_update_ue(redisContext* redis, struct du_collector_ue_stats* ue);
int redis_database_update_cell(redisContext* redis, struct du_collector_cell_stats* cell);
int insert_nr_mac_connector_struct_to_redis_database(redisContext** redis, struct du_collector_struct* collector_struct);

#endif