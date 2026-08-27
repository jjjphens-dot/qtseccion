#ifndef CSPORTMAN_H
#define CSPORTMAN_H
#include <qdatetime.h>
#include <qstring.h>
#include "cscore.h"
#include <QJsonObject>

class CSportMan
{
public:
    CSportMan();

    CSportMan(const CSportMan & man);
    CSportMan& operator= (const CSportMan& man);
    virtual ~CSportMan();

    // 读取、保存
    void SaveSportman(QJsonObject &obj) const;
    void ReadSportman(const QJsonObject &obj);

    //运动员个人数据
    int			 m_number;
    QString		 m_name;
    QDate        m_date;
    float        m_height;
    float        m_weight;

    CScore       m_score;

    int          m_place;
};

#endif // CSPORTMAN_H
