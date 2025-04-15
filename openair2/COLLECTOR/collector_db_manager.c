#include <stdio.h>
#include <time.h>
#include <sqlite3.h>
#include "du_collector.h"
#include "collector_db_manager.h"

#define REDIS_DB_FLUSH_LIMIT 180

extern struct du_collector_struct du_collector_struct_current;
extern struct du_collector_struct du_collector_struct_previous;
extern bool intervalChangedFlag;

int connect_redis_database(redisContext** redis, const char* host, uint16_t port)
{
    *redis = redisConnect(host, port);
    if ((*redis) == NULL || (*redis)->err) {
        printf("Failed to connect to Redis: %s\n", (*redis)->errstr);
        return -1;
    }

    redisReply *reply = redisCommand((*redis), "FLUSHDB");
    if(reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        printf("Failed to flush Redis database\n");
        freeReplyObject(reply);
        redisFree((*redis));
        return EXIT_FAILURE;
    }
    freeReplyObject(reply);
    
    return 0;
}
int redis_database_update_cell(redisContext* redis, struct du_collector_cell_stats* cell)
{
  char cellKey[30];
  time_t current_time = time(NULL);
  char timestamp[20];
  strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&current_time));
  sprintf(cellKey, "%lu:%s", cell->cell_id, timestamp);

  redisReply* reply = redisCommand(redis, "EXISTS %s", cellKey);
  if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
    LOG_E(FC_COLLECTOR, "COLLECTOR: Redis Error Exist Check \n");
    freeReplyObject(reply);
    return -1;
  }

  int exists = reply->integer;
  freeReplyObject(reply);
  if (exists) {
    LOG_D(FC_COLLECTOR, "Updating existing cell!\n");
    // Update existing cell!
    reply = redisCommand(redis, "HSET cellKey:%s timestamp %s cell_id %d cell_pci %d du_id %d dl_prb_usg %f ul_prb_usg %f"
                                " ul_throughput %f dl_throughput %f",
        cellKey,
        timestamp,
        cell->cell_id,
        cell->cell_pci,
        cell->du_id,
        cell->dl_prb_usg,
        cell->ul_prb_usg,
        cell->ul_throughput,
        cell->dl_throughput);
  } else {
    // Insert new cell!
    LOG_D(FC_COLLECTOR, "Insert new cell!\n");

    reply = redisCommand(redis, "HMSET cellKey:%s timestamp %s cell_id %d cell_pci %d du_id %d dl_prb_usg %f ul_prb_usg %f"
                                " ul_throughput %f dl_throughput %f",
        cellKey,
        timestamp,
        cell->cell_id,
        cell->cell_pci,
        cell->du_id,
        cell->dl_prb_usg,
        cell->ul_prb_usg,
        cell->ul_throughput,
        cell->dl_throughput);
  }

  if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
    LOG_E(FC_COLLECTOR, "COLLECTOR: Redis Error with exist flag: %d \n", exists);
    freeReplyObject(reply);
    return -1;
  }

  freeReplyObject(reply);
  return 0;
}

int redis_database_update_ue(redisContext* redis, struct du_collector_ue_stats* ue)
{
  uint8_t CC_id = 0; // for now
  char ueKey[30];
  time_t current_time = time(NULL);
  char timestamp[20];
  strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&current_time));
  sprintf(ueKey, "%d:%d:%s", ue->crnti, CC_id, timestamp);

  // Check if the ue exists in Redis
  redisReply* reply = redisCommand(redis, "EXISTS %s", ueKey);
  if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
    LOG_E(FC_COLLECTOR, "COLLECTOR: Redis Error Exist Check \n");
    freeReplyObject(reply);
    return -1;
  }

  int exists = reply->integer;
  freeReplyObject(reply);
  if (exists) {
    // Update existing ue!
    LOG_D(FC_COLLECTOR, "Updating existing ue!\n");
    reply = redisCommand(redis,
                         "HSET ueKey:%s timestamp %s crnti %d cu_f1ap_id %d"
                         " du_f1ap_id %d dl_total_rbs %d ul_total_rbs %d"
                         " dl_total_bytes %lu ul_total_bytes %lu  dl_throughput %f ul_throughput %f",
                         ueKey,
                         timestamp,
                         ue->crnti,
                         ue->cu_f1ap_id,
                         ue->du_f1ap_id,
                         ue->current_stats.dl_total_rbs,
                         ue->current_stats.ul_total_rbs,
                         ue->current_stats.dl_total_bytes,
                         ue->current_stats.ul_total_bytes,
                         ue->dl_throughput,
                         ue->ul_throughput);

  } else {
    // Insert new ue!
    reply = redisCommand(redis,
                         "HMSET ueKey:%s timestamp %s crnti %d cu_f1ap_id"
                         " %d du_f1ap_id %d dl_total_rbs %d ul_total_rbs %d"
                         " dl_total_bytes %lu  ul_total_bytes %lu "
                         "dl_throughput %f ul_throughput %f",
                         ueKey,
                         timestamp,
                         ue->crnti,
                         ue->cu_f1ap_id,
                         ue->du_f1ap_id,
                         ue->current_stats.dl_total_rbs,
                         ue->current_stats.ul_total_rbs,
                         ue->current_stats.dl_total_bytes,
                         ue->current_stats.ul_total_bytes,
                         ue->dl_throughput,
                         ue->ul_throughput);
  }

  if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
    LOG_E(FC_COLLECTOR, "COLLECTOR: Redis Error with exist flag: %d \n", exists);
    freeReplyObject(reply);
    return -1;
  }

  freeReplyObject(reply);
  return 0;
}

int check_size_and_flush_db(redisContext* redis)
{
    redisReply *reply = redisCommand(redis, "DBSIZE");
    if (reply == NULL)
    {
      LOG_E(FC_COLLECTOR, "REDIS DB: Can not run DBSIZE command\n");
      return -1;
    }

    if (reply->type != REDIS_REPLY_INTEGER)
    {
      LOG_E(FC_COLLECTOR, "REDIS DB: DBSIZE Reply is not an integer\n");
      freeReplyObject(reply);
      return -1;        
    }

    int numDBElements = reply->integer;
    if (numDBElements <= REDIS_DB_FLUSH_LIMIT)
        return 0;

    redisReply *flushReply = redisCommand(redis, "FLUSHDB");
    if (flushReply == NULL)
    {
      LOG_E(FC_COLLECTOR, "REDIS DB: FLUSH DB is failed!\n");
      return -1;          
    }

    freeReplyObject(flushReply);
    LOG_D(FC_COLLECTOR, "REDIS DB: DB is FLUSHED because of LIMIT!\n");
    return 0;
}

void copy_mac_collector_previous_stats()
{
    uint8_t ueId = 0;
    DU_Collector_UE_Iterator(du_collector_struct_current.ue_list, UE)
    {
      if (UE != NULL) {
        du_collector_ue_stats_t* UE_prev = (du_collector_ue_stats_t*)calloc(1, sizeof(du_collector_ue_stats_t));
        UE_prev->crnti = UE->crnti;
        UE_prev->previous_stats.dl_total_rbs = UE->previous_stats.dl_total_rbs;
        UE_prev->previous_stats.ul_total_rbs = UE->previous_stats.ul_total_rbs;
        UE_prev->previous_stats.dl_total_bytes = UE->previous_stats.dl_total_bytes;
        UE_prev->previous_stats.ul_total_bytes = UE->previous_stats.ul_total_bytes;
        du_collector_struct_previous.ue_list[ueId] = UE_prev;
        ueId++;
      }
    }
}

int insert_nr_mac_connector_struct_to_redis_database(redisContext** redis, struct du_collector_struct* collector_struct)
{
    copy_mac_collector_previous_stats();

    if (intervalChangedFlag == true) {
        intervalChangedFlag = false;
        return 0;
    }

    int size_check = check_size_and_flush_db(*redis);
    if (size_check != 0)
    {
      LOG_E(FC_COLLECTOR, "REDIS DB: Size Check Failed!\n");
      return -1;
    }

    DU_Collector_UE_Iterator(collector_struct->ue_list, UE)
    {
      if (UE != NULL) {
        int rc = redis_database_update_ue(*redis, UE);
        if (rc != 0) {
          LOG_E(FC_COLLECTOR, "Can not insert ue data to redis database\n");
          return rc;
        }
      }
    }

    DU_Collector_Cell_Iterator(collector_struct->cell_list, Cell)
    {
      if (Cell != NULL) {
        int rc = redis_database_update_cell(*redis, Cell);
        if (rc != 0) {
          LOG_E(FC_COLLECTOR, "Can not insert cell data to redis database\n");
          return rc;
        }
      }
    }

    return 0;
}

// ---- SQLITE FUNCTIONS ---- //
// void create_connector_database(sqlite3** db)
// {
//     int rc = sqlite3_open("../../../../batuhan/collector_db.db", &(*db));
//     if (rc != SQLITE_OK) {
//         LOG_E(FC_COLLECTOR,"Cannot open database: %s\n", sqlite3_errmsg(*db));
//         sqlite3_close(*db);
//         exit(1);
//     }

//     char* sql = "DROP TABLE IF EXISTS ue_database; CREATE TABLE IF NOT EXISTS ue_database (id INTEGER PRIMARY KEY
//     AUTOINCREMENT,timestamp DATETIME, rnti INT,total_rbs INT, curr_rbs INT);"; rc = sqlite3_exec(*db, sql, NULL, NULL, NULL); if
//     (rc != SQLITE_OK) {
//         LOG_E(FC_COLLECTOR,"Cannot create table: %s\n", sqlite3_errmsg(*db));
//         sqlite3_close(*db);
//         exit(1);
//     }
//     LOG_I(FC_COLLECTOR, "DATABASE is created!\n");
// }

// int insert_ue_object(struct NR_mac_collector_UE_info* ue, sqlite3* db)
// {
//     time_t current_time = time(NULL);
//     char timestamp[20];
//     strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", localtime(&current_time));

//     const int total_rbs =  ue->dl_total_rbs;
//     const int curr_rbs  =  ue->dl_current_rbs;
//     const int rnti = ue->rnti;
//     if (db == NULL){
//         LOG_W(FC_COLLECTOR, "DB is NULL\n");
//     }
//     char *sql = sqlite3_mprintf("INSERT INTO ue_database (timestamp, rnti, curr_rbs, total_rbs) VALUES ('%q', %d, %d, %d);",
//     timestamp, rnti, curr_rbs, total_rbs); int rc = sqlite3_exec(db, sql, NULL, NULL, NULL); sqlite3_free(sql);

//     return rc;
// }

// int insert_nr_mac_connect_struct(struct NR_mac_collector_struct* collector_struct, sqlite3** db)
// {
//     int ueNumber = 0;

//     while (collector_struct->ueList[ueNumber] != NULL)
//     {
//         int rc = insert_ue_object(collector_struct->ueList[ueNumber], *db);
//         if (rc != SQLITE_OK) {
//             fprintf(stderr, "Cannot insert data for ue :%d is : %s\n",ueNumber, sqlite3_errmsg(*db));
//             return rc;
//         }
//         ueNumber++;
//     }
//     return SQLITE_OK;
// }
