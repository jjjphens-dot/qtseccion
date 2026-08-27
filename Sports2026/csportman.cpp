#include "csportman.h"

CSportMan::CSportMan()
{

}
CSportMan::~CSportMan()
{

}

CSportMan::CSportMan(const CSportMan & man)
{
    m_number = man.m_number;
    m_name = man.m_name;
    m_date = man.m_date;
    m_height = man.m_height;
    m_weight = man.m_weight;
    m_score = man.m_score;
    m_place = man.m_place;
}

CSportMan& CSportMan::operator= (const CSportMan& man)
{
    m_number = man.m_number;
    m_name = man.m_name;
    m_date = man.m_date;
    m_height = man.m_height;
    m_weight = man.m_weight;
    m_score = man.m_score;
    m_place = man.m_place;
    return *this;
}

void CSportMan::SaveSportman(QJsonObject &obj) const
{
    obj.insert("number", m_number);
    obj.insert("name", m_name);
    obj.insert("birthDate", m_date.toString("yyyy-MM-dd"));
    obj.insert("height", double(m_height));
    obj.insert("weight", double(m_weight));

    QJsonObject results;
    m_score.WriteResultsToJson(results);
    obj.insert("results", results);
}

void CSportMan::ReadSportman(const QJsonObject &obj)
{
    m_number = obj.value("number").toInt(-1);
    m_name   = obj.value("name").toString();
    m_date   = QDate::fromString(obj.value("birthDate").toString(), "yyyy-MM-dd");
    m_height = float(obj.value("height").toDouble(0.0));
    m_weight = float(obj.value("weight").toDouble(0.0));
    m_place  = 0;
    m_score.ReadResultsFromJson(obj.value("results").toObject());
}
