#ifndef SPORTSMANINFOTABLE_H
#define SPORTSMANINFOTABLE_H
#include "csportman.h"
#include <QVector>

class SportsManInfoTable
{
public:
    SportsManInfoTable();

    void  CalculateRank();        // 计算排名，结果存入 m_place
    bool  ReadSportsmanFromFile(const QString &aReadFileName);
    bool  SaveSportsmanToFile(const QString &aSaveFileName);
    bool  AddSportman(CSportMan & sportman); // 返回false表示已满或编号重复
    CSportMan & GetSportMan(int index);
    int   FindSportmanByNumber(int number) const; // 按编号查找，返回下标；未找到返回-1
    int   GetSportsmanNum();
    void  Clear();

    static const int MAX_SPORTSMAN = 5000;

protected:
    QVector<CSportMan> m_sportmans;
    int m_number;
};

#endif // SPORTSMANINFOTABLE_H