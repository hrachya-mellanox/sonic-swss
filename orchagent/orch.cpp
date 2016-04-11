#include "orch.h"
#include "logger.h"

using namespace swss;

Orch::Orch(DBConnector *db, string tableName) :
    m_db(db)
{
    Consumer c_info(new ConsumerTable(m_db, tableName));
    m_consumer_map.insert(ConsumerMapPair(tableName, c_info));
}

Orch::Orch(DBConnector *db, vector<string> &tableNames) :
    m_db(db)
{
    for( auto it = tableNames.begin(); it != tableNames.end(); it++) {
        Consumer c_info(new ConsumerTable(m_db, *it));
        m_consumer_map.insert(ConsumerMapPair(*it, c_info));
    }
}

Orch::~Orch()
{
    delete(m_db);
    for(auto it : m_consumer_map) {
        delete it.second.m_consumer;
    }
}

void Orch::getSelectables(_out_ std::vector<Selectable*> &consumers)
{
    consumers.clear();
    for(auto it : m_consumer_map) {
        consumers.push_back(it.second.m_consumer);
    }
}
 
bool Orch::is_owned_consumer(ConsumerTable*consumer) const
{
    for(auto it : m_consumer_map) {
        if(it.second.m_consumer == consumer) {
            return true;
        }
    }
    return false;
}

bool Orch::execute(string tableName)
{
    auto consumer_it = m_consumer_map.find(tableName);
    if(consumer_it == m_consumer_map.end()) {
        SWSS_LOG_ERROR("Unrecognized tableName:%s\n", tableName.c_str());
        return false;
    }
    Consumer& consumer = consumer_it->second;

    KeyOpFieldsValuesTuple new_data;
    consumer.m_consumer->pop(new_data);

    string key = kfvKey(new_data);
    string op  = kfvOp(new_data);

#ifdef DEBUG
    string debug = "Table : " + consumer.m_consumer.getTableName() + " key : " + kfvKey(new_data) + " op : "  + kfvOp(new_data);
    for (auto i = kfvFieldsValues(new_data).begin(); i != kfvFieldsValues(new_data).end(); i++)
        debug += " " + fvField(*i) + " : " + fvValue(*i);
    SWSS_LOG_DEBUG("%s\n", debug.c_str());
#endif

    /* If a new task comes or if a DEL task comes, we directly put it into consumer.m_toSync map */
    if ( consumer.m_toSync.find(key) == consumer.m_toSync.end() || op == DEL_COMMAND)
    {
       consumer.m_toSync[key] = new_data;
    }
    /* If an old task is still there, we combine the old task with new task */
    else
    {
        KeyOpFieldsValuesTuple existing_data = consumer.m_toSync[key];

        auto new_values      = kfvFieldsValues(new_data);
        auto existing_values = kfvFieldsValues(existing_data);


        for (auto it = new_values.begin(); it != new_values.end(); it++)
        {
            string field = fvField(*it);
            string value = fvValue(*it);

            auto iu = existing_values.begin();
            while (iu != existing_values.end())
            {
                string ofield = fvField(*iu);
                if (field == ofield)
                    iu = existing_values.erase(iu);
                else
                    iu++;
            }
            existing_values.push_back(FieldValueTuple(field, value));
        }
        consumer.m_toSync[key] = KeyOpFieldsValuesTuple(key, op, existing_values);
    }

    doTask(consumer);
    return true;
}
