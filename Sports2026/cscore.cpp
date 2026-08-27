#include "cscore.h"
#include <cmath>

CScore::CScore(const CScore & score)
{
    for (int i = 0; i < 5; i++)
        m_record[i] = score.m_record[i];
    m_totalscore = score.m_totalscore;
}

CScore& CScore::operator= (const CScore & score)
{
    for (int i = 0; i < 5; i++)
        m_record[i] = score.m_record[i];
    m_totalscore = score.m_totalscore;
    return *this;
}

CScore::CScore()
{
    m_record[0].m_sportname = QStringLiteral("100米");
    m_record[1].m_sportname = QStringLiteral("110米栏");
    m_record[2].m_sportname = QStringLiteral("1500米");
    m_record[3].m_sportname = QStringLiteral("跳高");
    m_record[4].m_sportname = QStringLiteral("铅球");

    for (int i = 0; i < 5; i++)
    {
        m_record[i].m_record = 0.0f;
        m_record[i].m_score = 0.0f;
    }
    m_totalscore = 0.0f;
}

CScore::~CScore()
{
}

void CScore::WriteResultsToJson(QJsonObject &results) const
{
    for (int i = 0; i < 5; i++)
        results.insert(m_record[i].m_sportname, double(m_record[i].m_record));
}

void CScore::ReadResultsFromJson(const QJsonObject &results)
{
    // 按项目名匹配读取
    for (int i = 0; i < 5; i++)
        m_record[i].m_record = float(results.value(m_record[i].m_sportname).toDouble(0.0));
}

float CScore::GetLinear(float leftR, float rightR, float leftS, float rightS, float record)
{
    float denom = std::fabs(rightR - leftR);
    if (denom < 1e-6f)
        return leftS;
    return std::fabs(record - leftR) / denom * std::fabs(rightS - leftS) + leftS;
}

float CScore::GetScore(const float* gradeTable, const float* scoreTable, float record, bool lowerIsBetter)
{
    if (record <= 0.0f)
        return 0.0f;

    if (lowerIsBetter)
    {
        // 径赛项目：成绩越小越好（100米、110米栏、1500米）
        if (record >= gradeTable[0])
            return scoreTable[0];
        for (int i = 0; i < 5; i++)
        {
            if (record < gradeTable[i] && record >= gradeTable[i + 1])
                return GetLinear(gradeTable[i], gradeTable[i + 1], scoreTable[i], scoreTable[i + 1], record);
        }
        if (record < gradeTable[5])
            return scoreTable[5];
    }
    else
    {
        // 田赛项目：成绩越大越好（跳高、铅球）
        if (record <= gradeTable[0])
            return scoreTable[0];
        for (int i = 0; i < 5; i++)
        {
            if (record > gradeTable[i] && record <= gradeTable[i + 1])
                return GetLinear(gradeTable[i], gradeTable[i + 1], scoreTable[i], scoreTable[i + 1], record);
        }
        if (record > gradeTable[5])
            return scoreTable[5];
    }

    return 0.0f;
}

void CScore::CalculateTotalScore()
{
    // ========== 成绩表与分数表（静态常量） ==========

    // 1. 100米
    static const float grade100m[6]  = { 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.5f };
    static const float score100m[6]  = { 300, 400, 600, 900, 1300, 1600 };

    // 2. 110米栏
    static const float grade110mh[6] = { 17.0f, 16.0f, 15.0f, 14.0f, 13.0f, 12.0f };
    static const float score110mh[6] = { 300, 400, 600, 900, 1300, 1700 };

    // 3. 1500米
    static const float grade1500m[6] = { 6.0f, 5.5f, 5.0f, 4.5f, 4.0f, 3.5f };
    static const float score1500m[6] = { 200, 300, 500, 800, 1200, 1700 };

    // 4. 跳高
    static const float gradeJump[6]  = { 1.70f, 1.90f, 2.10f, 2.25f, 2.35f, 2.45f };
    static const float scoreJump[6]  = { 200, 300, 500, 800, 1200, 1700 };

    // 5. 铅球
    static const float gradeShot[6]  = { 14.0f, 16.0f, 18.0f, 20.0f, 22.0f, 24.0f };
    static const float scoreShot[6]  = { 100, 200, 400, 700, 1100, 1600 };

    struct EventConfig {
        const float* grades;
        const float* scores;
        bool lowerIsBetter;
    };

    static const EventConfig events[5] = {
        { grade100m,  score100m,  true  },  // 100米
        { grade110mh, score110mh, true  },  // 110米栏
        { grade1500m, score1500m, true  },  // 1500米
        { gradeJump,  scoreJump,  false },  // 跳高
        { gradeShot,  scoreShot,  false },  // 铅球
    };

    m_totalscore = 0.0f;
    for (int i = 0; i < 5; i++)
    {
        m_record[i].m_score = GetScore(events[i].grades, events[i].scores,
                                       m_record[i].m_record, events[i].lowerIsBetter);
        m_totalscore += m_record[i].m_score;
    }
}