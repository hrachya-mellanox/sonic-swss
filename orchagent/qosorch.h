#ifndef SWSS_QOSORCH_H
#define SWSS_QOSORCH_H

#include <map>
#include "orch.h"
#include "portsorch.h"

const std::string tc_to_pg_map_field_name           = "tc_to_pg_map";
const std::string pfc_to_pg_map_name                = "pfc_to_pg_map";
const std::string pfc_to_queue_map_name             = "pfc_to_queue_map";
const std::string pfc_enable_name                   = "pfc_enable";
const std::string tc_to_queue_field_name            = "tc_to_queue_map";
const std::string dscp_to_tc_field_name             = "dscp_to_tc_map";
const std::string scheduler_field_name              = "scheduler";
const std::string wred_profile_field_name           = "wred_profile";
const std::string yellow_max_threshold_field_name   = "yellow_max_threshold";
const std::string green_max_threshold_field_name    = "green_max_threshold";
const std::string red_max_threshold_field_name      = "red_max_threshold";
const std::string wred_green_enable_field_name      = "wred_green_enable";
const std::string wred_yellow_enable_field_name     = "wred_yellow_enable";
const std::string wred_red_enable_field_name        = "wred_red_enable";

const std::string scheduler_algo_type_field_name    = "type";
const std::string scheduler_algo_DWRR               = "DWRR";
const std::string scheduler_algo_WRR                = "WRR";
const std::string scheduler_algo_STRICT             = "STRICT";
const std::string scheduler_weight_field_name       = "weight";
const std::string scheduler_priority_field_name     = "priority";

const std::string ecn_field_name                    = "ecn";
const std::string ecn_none                          = "ecn_none";
const std::string ecn_green                         = "ecn_green";
const std::string ecn_yellow                        = "ecn_yellow";
const std::string ecn_red                           = "ecn_red";
const std::string ecn_green_yellow                  = "ecn_green_yellow";
const std::string ecn_green_red                     = "ecn_green_red";
const std::string ecn_yellow_red                    = "ecn_yellow_red";
const std::string ecn_all                           = "ecn_all";

class QosMapHandler
{
public:
    task_process_status processWorkItem(Consumer& consumer);
    virtual bool isValidTable(string &tableName) = 0;
    virtual bool convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes) = 0;
    virtual void freeAttribResources(std::vector<sai_attribute_t> &attributes) = 0;
    virtual bool modifyQosMap(sai_object_id_t, std::vector<sai_attribute_t> &attributes) = 0;
    virtual sai_object_id_t addQosMap(std::vector<sai_attribute_t> &attributes) = 0;
    virtual bool removeQosMap(sai_object_id_t sai_object) = 0;
};

class DscpToTcMapHandler : public QosMapHandler
{
public:
    bool isValidTable(string &tableName);
    bool convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes);
    void freeAttribResources(std::vector<sai_attribute_t> &attributes);
    bool modifyQosMap(sai_object_id_t, std::vector<sai_attribute_t> &attributes);
    sai_object_id_t addQosMap(std::vector<sai_attribute_t> &attributes);
    bool removeQosMap(sai_object_id_t sai_object);
};

class TcToQueueMapHandler : public QosMapHandler
{
public:
    bool isValidTable(string &tableName);
    bool convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes);
    void freeAttribResources(std::vector<sai_attribute_t> &attributes);
    bool modifyQosMap(sai_object_id_t, std::vector<sai_attribute_t> &attributes);
    sai_object_id_t addQosMap(std::vector<sai_attribute_t> &attributes);
    bool removeQosMap(sai_object_id_t sai_object);
};

class WredMapHandler : public QosMapHandler
{
public:
    bool isValidTable(string &tableName);
    bool convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes);
    void freeAttribResources(std::vector<sai_attribute_t> &attributes);
    bool modifyQosMap(sai_object_id_t, std::vector<sai_attribute_t> &attributes);
    sai_object_id_t addQosMap(std::vector<sai_attribute_t> &attributes);
    bool removeQosMap(sai_object_id_t sai_object);
protected:    
    bool convertEcnMode(string str, sai_ecn_mark_mode_t &ecn_val);
    bool convertBool(string str, bool &val);
};


class TcToPgHandler : public QosMapHandler
{
public:
    bool isValidTable(string &tableName);
    bool convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes);
    void freeAttribResources(std::vector<sai_attribute_t> &attributes);
    bool modifyQosMap(sai_object_id_t, std::vector<sai_attribute_t> &attributes);
    sai_object_id_t addQosMap(std::vector<sai_attribute_t> &attributes);
    bool removeQosMap(sai_object_id_t sai_object);
};

class PfcPrioToPgHandler : public QosMapHandler
{
public:
    bool isValidTable(string &tableName);
    bool convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes);
    void freeAttribResources(std::vector<sai_attribute_t> &attributes);
    bool modifyQosMap(sai_object_id_t, std::vector<sai_attribute_t> &attributes);
    sai_object_id_t addQosMap(std::vector<sai_attribute_t> &attributes);
    bool removeQosMap(sai_object_id_t sai_object);
};

class PfcToQueueHandler : public QosMapHandler
{
public:
    bool isValidTable(string &tableName);
    bool convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes);
    void freeAttribResources(std::vector<sai_attribute_t> &attributes);
    bool modifyQosMap(sai_object_id_t, std::vector<sai_attribute_t> &attributes);
    sai_object_id_t addQosMap(std::vector<sai_attribute_t> &attributes);
    bool removeQosMap(sai_object_id_t sai_object);
};

class QosOrch : public Orch
{
public:
    QosOrch(DBConnector *db, vector<string> &tableNames, PortsOrch *portsOrch);

    static type_map& getTypeMap();
    static type_map m_qos_type_maps;
private:
    virtual void doTask(Consumer& consumer);

    typedef task_process_status (QosOrch::*qos_table_handler)(Consumer& consumer);
    typedef std::map<std::string, qos_table_handler> qos_table_handler_map;
    typedef std::pair<string, qos_table_handler> qos_handler_pair;

    void initTableHandlers();
    task_process_status handleDscpToTcTable(Consumer& consumer);
    task_process_status handleTcToQueueTable(Consumer& consumer);
    task_process_status handleSchedulerTable(Consumer& consumer);
    task_process_status handleQueueTable(Consumer& consumer);
    bool applyWredProfileToQueue(Port &port, size_t queue_ind, sai_object_id_t sai_wred_profile);
    bool applySchedulerToQueueSchedulerGroup(Port &port, size_t queue_ind, sai_object_id_t scheduler_profile_id);
    task_process_status handlePortQosMapTable(Consumer& consumer);
    bool applyMapToPort(Port &port, sai_attr_id_t attr_id, sai_object_id_t sai_dscp_to_tc_map);
    task_process_status handleWredProfileTable(Consumer& consumer);
    task_process_status ResolveMapAndApplyToPort(
        Port                    &port,
        sai_port_attr_t         port_attr, 
        string                  field_name, 
        KeyOpFieldsValuesTuple  &tuple, 
        string                  op);

    task_process_status handleTcToPgTable(Consumer& consumer);
    task_process_status handlePfcPrioToPgTable(Consumer& consumer);
    task_process_status handlePfcToQueueTable(Consumer& consumer);

private:
    PortsOrch *m_portsOrch;
    qos_table_handler_map m_qos_handler_map;
};
#endif /* SWSS_QOSORCH_H */
