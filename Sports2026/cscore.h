#ifndef CSCORE_H
#define CSCORE_H
#include <qstring.h>
#include <QJsonObject>

struct Record
{
    QString m_sportname;
    float   m_score;
    float   m_record;
};

class CScore
{
public:
    CScore();
    virtual ~CScore();

    CScore(const CScore & score);
    CScore& operator= (const CScore & score);
    void CalculateTotalScore(); // 计算总分

    // 读取、保存分数
    void WriteResultsToJson(QJsonObject &results) const;
    void ReadResultsFromJson(const QJsonObject &results);

    Record  m_record[5];
    float   m_totalscore;

private:
    // 根据成绩表线性插值计算分数
    float GetScore(const float* gradeTable, const float* scoreTable, float record, bool lowerIsBetter);
    // 线性插值
    float GetLinear(float leftR, float rightR, float leftS, float rightS, float record);
};

#endif // CSCORE_H