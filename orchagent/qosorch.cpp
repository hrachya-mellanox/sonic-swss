#include "sai.h"

#include "qosorch.h"
#include "logger.h"

#include <sstream>
#include <iostream>

extern sai_port_api_t      *sai_port_api;
extern sai_queue_api_t     *sai_queue_api;
extern sai_scheduler_api_t *sai_scheduler_api;
extern sai_wred_api_t      *sai_wred_api;
extern sai_qos_map_api_t   *sai_qos_map_api;

namespace swss {


qos_type_map QosOrch::m_qos_type_maps = {
    {APP_DSCP_TO_TC_MAP_TABLE_NAME,  new qos_object_map()},
    {APP_TC_TO_QUEUE_MAP_TABLE_NAME, new qos_object_map()},
    {APP_SCHEDULER_TABLE_NAME,       new qos_object_map()},
    {APP_WRED_PROFILE_TABLE_NAME,    new qos_object_map()}
};


bool QosMapHandler::processWorkItem(Consumer& consumer)
{
    sai_object_id_t         sai_object          = SAI_NULL_OBJECT_ID;
    auto                    it                  = consumer.m_toSync.begin();
    KeyOpFieldsValuesTuple  tuple               = it->second;
    string                  qos_object_name     = kfvKey(tuple);
    string                  qos_map_type_name   = consumer.m_consumer->getTableName();
    string                  op                  = kfvOp(tuple);

    if(!isValidTable(qos_map_type_name)) {
        return false;
    }

    if(QosOrch::getTypeMap()[qos_map_type_name]->find(qos_object_name) != QosOrch::getTypeMap()[qos_map_type_name]->end()) {
        sai_object = (*(QosOrch::getTypeMap()[qos_map_type_name]))[qos_object_name];
    }
    if(op == SET_COMMAND) {
        std::vector<sai_attribute_t> attributes;
        if(!convertFieldValuesToAttributes(tuple, attributes)) {
            return false;
        }
        if(SAI_NULL_OBJECT_ID != sai_object) {
            if(!modifyExistingSaiObject(sai_object, attributes)) {
                SWSS_LOG_ERROR("Failed to set settings to existing dscp_to_tc map. db name:%s sai object:%llx", 
                    qos_object_name.c_str(), sai_object);
                freeAttribResources(attributes);
                return false;
            }
        }
        else {
            sai_object = createSaiObject(attributes);
            if(sai_object == SAI_NULL_OBJECT_ID) {
                SWSS_LOG_ERROR("Failed to create dscp_to_tc map. db name:%s", qos_object_name.c_str());
                freeAttribResources(attributes);
                return false;
            }
            (*(QosOrch::getTypeMap()[qos_map_type_name]))[qos_object_name] = sai_object;
        }        
        freeAttribResources(attributes);
    }    
    if(op == DEL_COMMAND) {
        if(SAI_NULL_OBJECT_ID == sai_object) {
            SWSS_LOG_ERROR("Object with name:%s not found.\n", qos_object_name.c_str());            
            return false;
        }
        if(!deleteSaiObject(sai_object)) {
            SWSS_LOG_ERROR("Failed to remove dscp_to_tc map. db name:%s sai object:%llx", qos_object_name.c_str(), sai_object);            
            return false;
        }
        auto it_to_delete = (QosOrch::getTypeMap()[qos_map_type_name])->find(qos_object_name);
        (QosOrch::getTypeMap()[qos_map_type_name])->erase(it_to_delete);
    }
    return true;

}

void DscpToTcMapHandler::freeAttribResources(std::vector<sai_attribute_t> &attributes)
{
    delete[] attributes[0].value.qosmap.list;
}

bool DscpToTcMapHandler::isValidTable(string &tableName)
{
    if(tableName != APP_DSCP_TO_TC_MAP_TABLE_NAME) {
        SWSS_LOG_ERROR("invalid table type passed in %s, expected:%s\n", tableName.c_str(), APP_DSCP_TO_TC_MAP_TABLE_NAME);
        return false;
    }
    return true;
}
bool DscpToTcMapHandler::convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes)
{
    sai_attribute_t list_attr;
    sai_qos_map_list_t dscp_map_list;
    dscp_map_list.count = kfvFieldsValues(tuple).size();
    dscp_map_list.list = new sai_qos_map_t[dscp_map_list.count];
    uint32_t ind = 0;
    for (auto i = kfvFieldsValues(tuple).begin(); i != kfvFieldsValues(tuple).end(); i++, ind++) {
        dscp_map_list.list[ind].key.dscp = std::stoi(fvField(*i));
        dscp_map_list.list[ind].value.tc = std::stoi(fvValue(*i));
#ifdef MLNXSAI
        dscp_map_list.list[ind].value.color = SAI_PACKET_COLOR_GREEN;
#endif
    }
    list_attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
    list_attr.value.qosmap.count = dscp_map_list.count;
    list_attr.value.qosmap.list = dscp_map_list.list;
    attributes.push_back(list_attr);
    return true;
}
bool DscpToTcMapHandler::modifyExistingSaiObject(sai_object_id_t sai_object, std::vector<sai_attribute_t> &attributes)
{
    sai_status_t sai_status = sai_qos_map_api->set_qos_map_attribute(sai_object, &attributes[0]);
    if(SAI_STATUS_SUCCESS != sai_status) {
        SWSS_LOG_ERROR("Failed to modify dscp_to_tc map. status:%d", sai_status);
        return false;
    }
    return true;
}
sai_object_id_t DscpToTcMapHandler::createSaiObject(std::vector<sai_attribute_t> &attributes)
{
    sai_status_t sai_status;
    sai_object_id_t sai_object;
    sai_attribute_t qos_map_attrs[2];
    qos_map_attrs[0].id = SAI_QOS_MAP_ATTR_TYPE;
#ifdef MLNXSAI
    qos_map_attrs[0].value.u32 = SAI_QOS_MAP_DSCP_TO_TC_AND_COLOR;
#else
    qos_map_attrs[0].value.u32 = SAI_QOS_MAP_DSCP_TO_TC;
#endif
    qos_map_attrs[1].id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
    qos_map_attrs[1].value.qosmap.count = attributes[0].value.qosmap.count;
    qos_map_attrs[1].value.qosmap.list = attributes[0].value.qosmap.list;
    sai_status = sai_qos_map_api->create_qos_map(&sai_object, 2, qos_map_attrs);
    if(SAI_STATUS_SUCCESS != sai_status) {
        SWSS_LOG_ERROR("Failed to create dscp_to_tc map. status:%d", sai_status);
        return SAI_NULL_OBJECT_ID;
    }
    return sai_object;
}
bool DscpToTcMapHandler::deleteSaiObject(sai_object_id_t sai_object)
{
    sai_status_t sai_status = sai_qos_map_api->remove_qos_map(sai_object);
    if(SAI_STATUS_SUCCESS != sai_status) {
        SWSS_LOG_ERROR("Failed to remove dscp_to_tc map. status:%d", sai_status);
        return false;
    }
    return true;
}
bool QosOrch::handleDscpToTcTable(Consumer& consumer)
{
    DscpToTcMapHandler dscp_tc_handler;
    return dscp_tc_handler.processWorkItem(consumer);
}

void TcToQueueMapHandler::freeAttribResources(std::vector<sai_attribute_t> &attributes)
{
    delete[] attributes[0].value.qosmap.list;
}

bool TcToQueueMapHandler::isValidTable(string &tableName)
{
    if(tableName != APP_TC_TO_QUEUE_MAP_TABLE_NAME) {
        SWSS_LOG_ERROR("invalid table type passed in %s, expected:%s\n", tableName.c_str(), APP_TC_TO_QUEUE_MAP_TABLE_NAME);
        return false;
    }
    return true;
}
bool TcToQueueMapHandler::convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attributes)
{
    sai_attribute_t     list_attr;
    sai_qos_map_list_t  tc_map_list;
    tc_map_list.count = kfvFieldsValues(tuple).size();
    tc_map_list.list = new sai_qos_map_t[tc_map_list.count];
    uint32_t ind = 0;
    for (auto i = kfvFieldsValues(tuple).begin(); i != kfvFieldsValues(tuple).end(); i++, ind++) {
        tc_map_list.list[ind].key.tc = std::stoi(fvField(*i));
        tc_map_list.list[ind].value.queue_index = std::stoi(fvValue(*i));
    }
    list_attr.id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
    list_attr.value.qosmap.count = tc_map_list.count;
    list_attr.value.qosmap.list = tc_map_list.list;
    attributes.push_back(list_attr);
    return true;
}

bool TcToQueueMapHandler::modifyExistingSaiObject(sai_object_id_t sai_object, std::vector<sai_attribute_t> &attributes)
{
    sai_status_t sai_status = sai_qos_map_api->set_qos_map_attribute(sai_object, &attributes[0]);
    if(SAI_STATUS_SUCCESS != sai_status) {
        SWSS_LOG_ERROR("Failed to moify tc_to_queue map. status:%d", sai_status);
        return false;
    }
    return true;
}

sai_object_id_t TcToQueueMapHandler::createSaiObject(std::vector<sai_attribute_t> &attributes)
{
    sai_status_t sai_status;
    sai_object_id_t sai_object;
    sai_attribute_t qos_map_attrs[2];
    qos_map_attrs[0].id = SAI_QOS_MAP_ATTR_TYPE;
    qos_map_attrs[0].value.s32 = SAI_QOS_MAP_TC_TO_QUEUE;
    qos_map_attrs[1].id = SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST;
    qos_map_attrs[1].value.qosmap.count = attributes[0].value.qosmap.count;
    qos_map_attrs[1].value.qosmap.list = attributes[0].value.qosmap.list;
    sai_status = sai_qos_map_api->create_qos_map(&sai_object, 2, qos_map_attrs);
    if(SAI_STATUS_SUCCESS != sai_status) {
        SWSS_LOG_ERROR("Failed to create tc_to_queue map. status:%d", sai_status);
        return SAI_NULL_OBJECT_ID;
    }
    return sai_object;
}

bool TcToQueueMapHandler::deleteSaiObject(sai_object_id_t sai_object)
{
    sai_status_t sai_status = sai_qos_map_api->remove_qos_map(sai_object);
    if(SAI_STATUS_SUCCESS != sai_status) {
        SWSS_LOG_ERROR("Failed to remove tc_to_queue mapstatus%d", sai_status);
        return false;
    }
    return true;
}

bool QosOrch::handleTcToQueueTable(Consumer& consumer)
{
    TcToQueueMapHandler tc_queue_handler;
    return tc_queue_handler.processWorkItem(consumer);
}

void WredMapHandler::freeAttribResources(std::vector<sai_attribute_t> &attributes)
{
    delete[] attributes[0].value.qosmap.list;
}

bool WredMapHandler::isValidTable(string &tableName)
{
    if(tableName != APP_WRED_PROFILE_TABLE_NAME) {
        SWSS_LOG_ERROR("invalid table type passed in %s, expected:%s\n", tableName.c_str(), APP_WRED_PROFILE_TABLE_NAME);
        return false;
    }
    return true;
}
bool WredMapHandler::convertFieldValuesToAttributes(KeyOpFieldsValuesTuple &tuple, std::vector<sai_attribute_t> &attribs)
{
    sai_attribute_t attr;
    for (auto i = kfvFieldsValues(tuple).begin(); i != kfvFieldsValues(tuple).end(); i++) {
        if (fvField(*i) == yellow_max_threshold_field_name) {
            attr.id = SAI_WRED_ATTR_YELLOW_ENABLE;
            attr.value.booldata = true;
            attribs.push_back(attr);
    
            attr.id = SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD;
            attr.value.s32 = std::stoi(fvValue(*i));
            attribs.push_back(attr);
        }
        else if(fvField(*i) == green_max_threshold_field_name) {
            attr.id = SAI_WRED_ATTR_GREEN_ENABLE;
            attr.value.booldata = true;
            attribs.push_back(attr);
    
            attr.id = SAI_WRED_ATTR_GREEN_MAX_THRESHOLD;
            attr.value.s32 = std::stoi(fvValue(*i));
            attribs.push_back(attr);
        }
        else {
            SWSS_LOG_ERROR( "Unkonwn wred profile field:%s", fvField(*i).c_str());
            return false;
        }
    }
    return true;
}

bool WredMapHandler::modifyExistingSaiObject(sai_object_id_t sai_object, std::vector<sai_attribute_t> &attribs)
{
    sai_status_t sai_status;
    for(auto attr : attribs) {
        sai_status = sai_wred_api->set_wred_attribute(sai_object, &attr);
        if (sai_status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR( "Failed to set wred profile attribute, id:%d, status:%d", attr.id, sai_status);
            return false;
        }            
    }
    return true;
}

sai_object_id_t WredMapHandler::createSaiObject(std::vector<sai_attribute_t> &attribs)
{
    sai_status_t sai_status;
    sai_object_id_t sai_object;
    sai_status = sai_wred_api->create_wred_profile(&sai_object, attribs.size(), attribs.data());
    if (sai_status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR( "Failed to create wred profile: %d", sai_status);
        return false;
    }
    return sai_object;
}
bool WredMapHandler::deleteSaiObject(sai_object_id_t sai_object)
{
    sai_status_t sai_status;
    sai_status = sai_wred_api->remove_wred_profile(sai_object);
    if(SAI_STATUS_SUCCESS != sai_status) {
        SWSS_LOG_ERROR("Failed to remove scheduler profile, status:%d", sai_status);
        return false;
    }
    return true;
}

bool QosOrch::handleWredProfileTable(Consumer& consumer)
{
    WredMapHandler wred_handler;
    return wred_handler.processWorkItem(consumer);
}

QosOrch::QosOrch(DBConnector *db, vector<string> &tableNames, PortsOrch *portsOrch) :
    Orch(db, tableNames), m_portsOrch(portsOrch) 
{
    initTableHandlers();
};

qos_type_map& QosOrch::getTypeMap()
{
    return m_qos_type_maps;
}

void QosOrch::initTableHandlers()
{
    m_qos_handler_map.insert(qos_handler_pair(APP_DSCP_TO_TC_MAP_TABLE_NAME, &QosOrch::handleDscpToTcTable));
    m_qos_handler_map.insert(qos_handler_pair(APP_TC_TO_QUEUE_MAP_TABLE_NAME, &QosOrch::handleTcToQueueTable));
    m_qos_handler_map.insert(qos_handler_pair(APP_SCHEDULER_TABLE_NAME, &QosOrch::handleSchedulerTable));
    m_qos_handler_map.insert(qos_handler_pair(APP_QUEUE_TABLE_NAME, &QosOrch::handleQueueTable));
    m_qos_handler_map.insert(qos_handler_pair(APP_PORT_QOS_MAP_TABLE_NAME, &QosOrch::handlePortQosMapTable));
    m_qos_handler_map.insert(qos_handler_pair(APP_WRED_PROFILE_TABLE_NAME, &QosOrch::handleWredProfileTable));
}

// TODO: This needs to change. Scheduler profile should be applied to scheduler group which is
// directly attached to the queue.
bool QosOrch::handleSchedulerTable(Consumer& consumer)
{
    sai_status_t            sai_status;
    sai_object_id_t         sai_object = SAI_NULL_OBJECT_ID;
    auto                    it = consumer.m_toSync.begin();
    KeyOpFieldsValuesTuple  tuple = it->second;
    string                  qos_map_type_name = consumer.m_consumer->getTableName();
    string                  qos_object_name = kfvKey(tuple);
    string                  op = kfvOp(tuple);

    if(qos_map_type_name != APP_SCHEDULER_TABLE_NAME) {
        SWSS_LOG_ERROR("Key with invalid table type passed in %s, expected:%s\n", qos_map_type_name.c_str(), APP_SCHEDULER_TABLE_NAME);
        return false;
    }
    if(m_qos_type_maps[qos_map_type_name]->find(qos_object_name) != m_qos_type_maps[qos_map_type_name]->end()) {
        sai_object = (*(m_qos_type_maps[qos_map_type_name]))[qos_object_name];
        if(sai_object == SAI_NULL_OBJECT_ID) {
            SWSS_LOG_ERROR("Error sai_object must exist for key %s\n", qos_object_name.c_str());
            return false;
        }       
    }

    if(op == SET_COMMAND) {
        std::vector<sai_attribute_t> sai_attr_list;
        sai_attribute_t attr;
        for (auto i = kfvFieldsValues(tuple).begin(); i != kfvFieldsValues(tuple).end(); i++) {
            if (fvField(*i) == scheduler_algo_type_field_name) {                
                attr.id = SAI_SCHEDULER_ATTR_SCHEDULING_ALGORITHM;
                if(fvValue(*i) == scheduler_algo_DWRR) {
                    attr.value.s32 = SAI_SCHEDULING_DWRR;
                }
                else if(fvValue(*i) == scheduler_algo_WRR) {
                    attr.value.s32 = SAI_SCHEDULING_WRR;
                }
                else if(fvValue(*i) == scheduler_algo_PRIORITY) {// TODO: is this correct? should it be named STRICT?
                    attr.value.s32 = SAI_SCHEDULING_STRICT;
                }
                else {
                    SWSS_LOG_ERROR( "Unknown scheduler type value:%s", fvField(*i).c_str());
                    return false;
                }
                sai_attr_list.push_back(attr);
            }
            else if (fvField(*i) == scheduler_weight_field) {
                attr.id = SAI_SCHEDULER_ATTR_SCHEDULING_WEIGHT;
                attr.value.s32 = std::stoi(fvValue(*i));
                sai_attr_list.push_back(attr);                
            }
            else if (fvField(*i) == scheduler_priority_field) {// TODO: What does this map to?
                // TODO: Pull request for SAI_SCHEDULER_ATTR_PRIORITY attribute
            }
            else {
                SWSS_LOG_ERROR( "Unknown field:%s", fvField(*i).c_str());
                return false;
            }            
            if(SAI_NULL_OBJECT_ID != sai_object) {
                for(auto attr : sai_attr_list) {
                    sai_status = sai_scheduler_api->set_scheduler_attribute(sai_object, &attr);
                    if (sai_status != SAI_STATUS_SUCCESS)
                    {
                        SWSS_LOG_ERROR( "fail to set scheduler attribute, id:%d", attr.id);
                        return false;
                    }
                }            
            }
            else {
                sai_status = sai_scheduler_api->create_scheduler_profile(&sai_object, sai_attr_list.size(), sai_attr_list.data());
                if (sai_status != SAI_STATUS_SUCCESS)
                {
                    SWSS_LOG_ERROR( "fail to call sai_scheduler_api->create_scheduler_profile: %d", sai_status);
                    return false;
                }
                (*(m_qos_type_maps[qos_map_type_name]))[qos_object_name] = sai_object;
            }
        }
    }
    if(op == DEL_COMMAND) {
        if(SAI_NULL_OBJECT_ID == sai_object) {
            SWSS_LOG_ERROR("Object with name:%s not found.\n", qos_object_name.c_str());            
            return false;
        }
        // delete SAI object and map[qos_object_name] and 
        sai_status = sai_scheduler_api->remove_scheduler_profile(sai_object);
        if(SAI_STATUS_SUCCESS != sai_status) {
            SWSS_LOG_ERROR("Failed to remove scheduler profile. db name:%s sai object:%llx", qos_object_name.c_str(), sai_object);
            return false;
        }
        auto it_to_delete = (m_qos_type_maps[qos_map_type_name])->find(qos_object_name);
        (m_qos_type_maps[qos_map_type_name])->erase(it_to_delete);
    }
    return true;
}

bool QosOrch::applyObjectToQueue(Port &port, size_t queue_ind, sai_queue_attr_t queue_attr, sai_object_id_t sai_object)
{
        sai_attribute_t attr;
        sai_status_t    sai_status;
        sai_object_id_t queue = port.getQueue(queue_ind);
        if(SAI_NULL_OBJECT_ID == queue) {
            SWSS_LOG_ERROR("Invalid queue index specified:%d", queue_ind);
            return false;
        }
        attr.id = queue_attr;
        attr.value.oid = sai_object;
        // sai_object_type_query not implemented in sairedis?
    /*    sai_object_type_t object_type = sai_object_type_query(sai_dscp_to_tc_map); 
        if(SAI_OBJECT_TYPE_SCHEDULER_PROFILE != object_type) {
            SWSS_LOG_ERROR("Unexpected sai object type:%d, expected SAI_OBJECT_TYPE_QOS_MAPS\n", object_type);
            return false;
        }
    */    
        sai_status = sai_queue_api->set_queue_attribute(queue, &attr);
        if (sai_status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to set wred profile:%d", sai_status);
            return false;
        }
        return true;
}

bool QosOrch::tryApplyRefFieldToPortQueue(
    const string                &op, 
    const string                &map_field_name, 
    apply_map_to_port_queue_fn  fn, 
    Port                        &port, 
    size_t                      queue_index, 
    sai_queue_attr_t            queue_attr, 
    KeyOpFieldsValuesTuple      &tuple)
{
    sai_object_id_t sai_object;
    string          object_type;
    string          object_alias;
    bool            result = false;
    resolve_status  resolve_result = resolveFieldRefValue(map_field_name, tuple, object_type, object_alias, sai_object, true);
    switch(resolve_result) {
        case resolve_status::failure :
        case resolve_status::multiple_instances :
            return false;
            break;
        case resolve_status::success :
            if(op == SET_COMMAND) {
                result = (this->*fn)(port, queue_index, queue_attr, sai_object);
            }
            if(op == DEL_COMMAND) {
                // NOTE: The map is un-bound from the port. But the map itself still exists.
                result = (this->*fn)(port, queue_index, queue_attr, SAI_NULL_OBJECT_ID);                
            }
            break;
        case resolve_status::field_not_found :
            result = true;
            break;
    }
    return result;
}

// TODO: This needs to change - scheduler profile will be applied to scheduler group associated with the queue.
bool QosOrch::handleQueueTable(Consumer& consumer)
{
    auto it = consumer.m_toSync.begin();
    KeyOpFieldsValuesTuple tuple = it->second;
    Port port;
    string key = kfvKey(tuple);
    string op = kfvOp(tuple);
    size_t queue_ind = 0;
    vector<string> tokens;
    // sample "QUEUE_TABLE:ETHERNET4:1"
    if(!tokenizeString(key, delimiter, tokens)) {
        return false;
    }
    if(tokens.size() != 2) {
        SWSS_LOG_ERROR("malformed key:%s. Must contain 2 tokens\n", key.c_str());
        return false;
    }
    if(consumer.m_consumer->getTableName() != APP_QUEUE_TABLE_NAME) {
        SWSS_LOG_ERROR("Key with invalid table type passed in %s, expected:%s\n", key.c_str(), APP_QUEUE_TABLE_NAME);
        return false;
    }
    queue_ind = std::stoi(tokens[1]);
    if(!m_portsOrch->getPort(tokens[0], port)) {
        SWSS_LOG_ERROR("Port with alias:%s not found\n", tokens[0].c_str());
        return false;
    }
    if(!tryApplyRefFieldToPortQueue(op, scheduler_field_name, &QosOrch::applyObjectToQueue, port, queue_ind, SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID, tuple)) {
        SWSS_LOG_ERROR("Failed setting field:%s to port:%s, queue:%d, line:%d\n", scheduler_field_name.c_str(), port.m_alias.c_str(), queue_ind, __LINE__);
        return false;
    }
    if(!tryApplyRefFieldToPortQueue(op, wred_profile_field_name, &QosOrch::applyObjectToQueue, port, queue_ind, SAI_QUEUE_ATTR_WRED_PROFILE_ID, tuple)) {
        SWSS_LOG_ERROR("Failed setting field:%s to port:%s, queue:%d, line:%d\n", wred_profile_field_name.c_str(), port.m_alias.c_str(), queue_ind, __LINE__);
        return false;
    }
    return true;
}

bool QosOrch::tokenizeString(string str, const string &separator, vector<string> &tokens)
{
    if(0 == separator.size()) {
        SWSS_LOG_ERROR("Invalid separator passed in:%s\n", separator.c_str());
        return false;
    }
    if(string::npos == str.find(separator)) {
        SWSS_LOG_ERROR("Specified separator:%s not found in input:%s\n", separator.c_str(), str.c_str());
        return false;
    }
    istringstream ss(str);
    string tmp;
    while (getline(ss, tmp, separator[0])) {
        tokens.push_back(tmp);
    }
    return true;
}

/*
- Validates reference is has proper format which is [table_name:object_name]
- validates table_name exists
- validates object with object_name exists
*/
bool QosOrch::parseReference(string &ref, string &type_name, string &object_name)
{
    if(0 == ref.size()) {
        SWSS_LOG_ERROR("empty reference received\n");
        return false;
    }
    if((ref[0] != ref_start) && (ref[ref.size()-1] != ref_end)) {
        SWSS_LOG_ERROR("malformed reference:%s. Must be surrounded by [ ]\n", ref.c_str());
        return false;
    }
    vector<string> tokens;
    if(!tokenizeString(ref, delimiter, tokens)) {
        return false;
    }
    if(tokens.size() != 2) {
        SWSS_LOG_ERROR("malformed reference:%s. Must contain 2 tokens\n", ref.c_str());
        return false;
    }
    auto type_it = m_qos_type_maps.find(tokens[0]);
    if(type_it == m_qos_type_maps.end()) {
        SWSS_LOG_ERROR("not recognized type:%s\n", tokens[0].c_str());
        return false;
    }
    auto obj_map = m_qos_type_maps[tokens[0]];
    auto obj_it = obj_map->find(tokens[1]);
    if(obj_it == obj_map->end()) {
        SWSS_LOG_ERROR("map:%s does not contain object with name:%s\n", tokens[0].c_str(), tokens[1].c_str());
        return false;
    }
    type_name   = tokens[0];
    object_name = tokens[1];
    return true;
}

QosOrch::resolve_status QosOrch::resolveFieldRefValue(
    const string            &field_name, 
    KeyOpFieldsValuesTuple  &tuple, 
    string                  &obj_type,
    string                  &obj_alias,
    sai_object_id_t         &sai_object,
    bool                    is_singleton)
{
    size_t count = 0;
    for (auto i = kfvFieldsValues(tuple).begin(); i != kfvFieldsValues(tuple).end(); i++) {
        if (fvField(*i) == field_name) {
            if(is_singleton && (count > 1)) {
                SWSS_LOG_ERROR("Singleton field with name:%s must have only 1 instance, actual count:%d\n", field_name.c_str(), count);
                return resolve_status::multiple_instances;
            }
            string ref_type_name, object_name;
            if(!parseReference(fvValue(*i), ref_type_name, object_name)) {
                return resolve_status::failure;
            }
            sai_object = (*(m_qos_type_maps[ref_type_name]))[object_name];
            obj_alias = object_name;
            obj_type = ref_type_name;
            count++;
        }
    }
    if(0 == count) {
        SWSS_LOG_NOTICE("field with name:%s not found\n", field_name.c_str());
        return resolve_status::field_not_found;
    }
    return resolve_status::success;
}

bool QosOrch::applyMapToPort(Port &port, sai_attr_id_t attr_id, sai_object_id_t map)
{
    sai_attribute_t attr;
    sai_status_t    sai_status;
    // sai_object_type_query seems not implemented or exposed from sairedis?
/*    sai_object_type_t object_type = sai_object_type_query(sai_tc_to_queue_map);     
    if(SAI_OBJECT_TYPE_QOS_MAPS != object_type) {
        SWSS_LOG_ERROR("Unexpected sai object type:%d, expected SAI_OBJECT_TYPE_QOS_MAPS\n", object_type);
        return false;
    }
*/    
    attr.id = attr_id;
    attr.value.oid = map;
    if(SAI_STATUS_SUCCESS != (sai_status = sai_port_api->set_port_attribute(port.m_port_id, &attr))) {
        SWSS_LOG_ERROR("Failed setting sai object:%llx for port:%s, status:%s\n", map, port.m_alias.c_str(), sai_status);
        return false;
    }
    return true;
}

/* 
Find field specified by map_field_name and if present apply corresponding sai object to the port.
*/
bool QosOrch::tryApplyRefFieldToPort(
    const string            &op, 
    const string            &map_field_name, 
    Port                    &port, 
    sai_attr_id_t           attr_id, 
    apply_map_to_port_fn    fn, 
    KeyOpFieldsValuesTuple  &tuple)
{
    sai_object_id_t sai_object;
    string          object_type;
    string          object_alias;
    bool            result          = false;
    resolve_status  resolve_result  = resolveFieldRefValue(map_field_name, tuple, object_type, object_alias, sai_object, true);
    switch(resolve_result) {
        case resolve_status::failure :
        case resolve_status::multiple_instances :
            return false;
            break;
        case resolve_status::success :
            if(op == SET_COMMAND) {
                result = (this->*fn)(port, attr_id, sai_object);
            }
            if(op == DEL_COMMAND) {
                // NOTE: The map is un-bound from the port. But the map itself still exists.
                result = (this->*fn)(port, attr_id, SAI_NULL_OBJECT_ID);
            }
            break;
        case resolve_status::field_not_found :
            // Item not found is not an error case
            result = true;
            break;
    }
    return result;
}

bool QosOrch::handlePortQosMapTable(Consumer& consumer)
{
    auto it = consumer.m_toSync.begin();
    KeyOpFieldsValuesTuple tuple = it->second;
    Port port;
    string port_alias = kfvKey(tuple);
    string op = kfvOp(tuple);
    if(consumer.m_consumer->getTableName() != APP_PORT_QOS_MAP_TABLE_NAME) {
        SWSS_LOG_ERROR("Key with invalid table type passed in %s, expected:%s\n", port_alias.c_str(), APP_PORT_QOS_MAP_TABLE_NAME);
        return false;
    }
    if(!m_portsOrch->getPort(port_alias, port)) {
        SWSS_LOG_ERROR("Port with alias:% not found\n", port_alias.c_str());
        return false;
    }

    sai_attr_id_t attr_id;
#ifdef MLNXSAI
        attr_id = SAI_PORT_ATTR_QOS_DSCP_TO_TC_AND_COLOR_MAP;
#else
        attr_id = SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP;
#endif
    if(!tryApplyRefFieldToPort(op, dscp_to_tc_field_name, port, attr_id, &QosOrch::applyMapToPort, tuple)) {
        SWSS_LOG_ERROR("Failed setting field:%s to port:%s, line:%d\n", dscp_to_tc_field_name.c_str(), port.m_alias.c_str(), __LINE__);
        return false;
    }
    attr_id = SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP;
    if(!tryApplyRefFieldToPort(op, tc_to_queue_field_name, port, attr_id, &QosOrch::applyMapToPort, tuple)) {
        SWSS_LOG_ERROR("Failed setting field:%s to port:%s, line:%d\n", tc_to_queue_field_name.c_str(), port.m_alias.c_str(), __LINE__);
        return false;
    }
    return true;
}

void QosOrch::doTask(Consumer &consumer)
{
    if (consumer.m_toSync.empty()) {
        return;
    }
    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end()) {
        KeyOpFieldsValuesTuple tuple = it->second;
        //
        // make sure table is recognized, and we have handler for it
        //
        string qos_map_type_name = consumer.m_consumer->getTableName();
        if(m_qos_type_maps.find(qos_map_type_name) == m_qos_type_maps.end()) {
            SWSS_LOG_ERROR("Unrecognised qos table encountered:%s\n", qos_map_type_name.c_str());
            //return;//?? [question.6]should be return or continue to retry later?
            it++;
            continue;
        }
        if(m_qos_handler_map.find(qos_map_type_name) == m_qos_handler_map.end()) {
            SWSS_LOG_ERROR("No handler for key:%s found.\n", qos_map_type_name.c_str());
            it++;
            continue;
        }
        /*
        [question.8] Decision on erasing an item from m_toSync to not loop forever.
            When there is a failure in processing an item, need to decide - should leave it in m_toSync 
            to retry later, or should remove from m_to_sync.

            Returning bool may not be sufficient, may need to return different error codes
            based on which to decide to do consumer.m_toSync.erase() or it++ to retry later.
        */
        if(!(this->*(m_qos_handler_map[qos_map_type_name]))(consumer)) {
            SWSS_LOG_ERROR("Failed to handle task for table:%s\n", qos_map_type_name.c_str());
            //return;//?? [question.7]should be return or continue to retry later?
            it++;
            continue;
        }
        /*
        TODO: 
        [question.1] When an operation fails we're supposed to put the operation into the back of some queue mentioned
        by Shuotian. If a handler above fails, how should this be done?
        [question.2] Why PortsOrch is not in swss namespace?
        [question.5] Can there be several scheduler profiles attached to port[i].queue[j]?
        [question.9] 
        */
        /*
        [question.4] how about CPU port?
            Code is oqssyncer.cpp::configure_cpu_port() does following:
            
        {
            "SCHEDULER_TABLE:CPU_PORT_SCHEDULER" : {
                "algorithm":"strict",
                "weight": "35"
            },
            "OP": "SET"
        },
        {
            "QUEUE_TABLE:ETHERNET_CPU_PORT:0" : {
                "scheduler"     :   "[SCHEDULER_TABLE:CPU_PORT_SCHEDULER]"
            },
            "OP": "SET"
        }
        {
            "QUEUE_TABLE:ETHERNET_CPU_PORT:1" : {
                "scheduler"     :   "[SCHEDULER_TABLE:CPU_PORT_SCHEDULER]"
            },
            "OP": "SET"
        }
        . . .
        {
            "QUEUE_TABLE:ETHERNET_CPU_PORT:64" : {
                "scheduler"     :   "[SCHEDULER_TABLE:CPU_PORT_SCHEDULER]"
            },
            "OP": "SET"
        }
        */
        it = consumer.m_toSync.erase(it);
    }
}

}
