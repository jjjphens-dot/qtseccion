#include "sportsmaninfotable.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <algorithm>
#include <QMessageBox>

SportsManInfoTable::SportsManInfoTable()
{
    m_number = 0;
}

void SportsManInfoTable::CalculateRank()
{
    int total = m_sportmans.size();
    if (total == 0)
        return;

    // 创建索引数组并按总分降序排序
    QVector<int> indices(total);
    for (int i = 0; i < total; i++)
        indices[i] = i;

    std::sort(indices.begin(), indices.end(),
              [this](int a, int b) {
                  return m_sportmans[a].m_score.m_totalscore > m_sportmans[b].m_score.m_totalscore;
              });

    // 分配名次（处理并列）
    for (int i = 0; i < total; i++)
    {
        int rank;
        if (i > 0 &&
            m_sportmans[indices[i]].m_score.m_totalscore == m_sportmans[indices[i - 1]].m_score.m_totalscore)
        {
            rank = m_sportmans[indices[i - 1]].m_place; // 并列
        }
        else
        {
            rank = i + 1;
        }
        m_sportmans[indices[i]].m_place = rank;
    }
}

bool SportsManInfoTable::ReadSportsmanFromFile(const QString &aReadFileName)
{
    QFile aFile(aReadFileName);
    if (!aFile.exists())
        return false;
    if (!aFile.open(QIODevice::ReadOnly))
        return false;

    // 解析 JSON，语法错误给出具体位置提示
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(aFile.readAll(), &parseErr);
    aFile.close();
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
    {
        QMessageBox::warning(nullptr, QStringLiteral("格式错误"),
                             QStringLiteral("不是有效的 JSON 数据文件：%1").arg(parseErr.errorString()));
        return false;
    }

    m_sportmans.clear();
    m_number = 0;

    // 逐条解析运动员；编号/姓名/日期不合法的条目跳过，不影响其余数据
    const QJsonArray athletes = doc.object().value("athletes").toArray();
    for (const QJsonValue &v : athletes)
    {
        if (!v.isObject())
            continue;
        CSportMan tempSportman;
        tempSportman.ReadSportman(v.toObject());
        if (tempSportman.m_number <= 0 ||
            tempSportman.m_name.isEmpty() ||
            !tempSportman.m_date.isValid())
        {
            continue;   // 数据不完整，跳过该条
        }
        m_sportmans.push_back(tempSportman);
    }

    // 打开后按当前计分标准重新计算各人总分（总分/名次为派生数据，不落盘）
    for (int i = 0; i < m_sportmans.size(); i++)
        m_sportmans[i].m_score.CalculateTotalScore();

    // 打开后自动计算排名
    CalculateRank();
    return true;
}

bool SportsManInfoTable::SaveSportsmanToFile(const QString &aSaveFileName)
{
    QFile aFile(aSaveFileName);
    if (!aFile.open(QIODevice::WriteOnly))
        return false;

    QJsonArray athletes;
    for (int i = 0; i < m_sportmans.size(); i++)
    {
        QJsonObject obj;
        m_sportmans[i].SaveSportman(obj);
        athletes.append(obj);
    }
    QJsonObject root;
    root.insert("athletes", athletes);

    aFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    aFile.close();
    return true;
}

bool SportsManInfoTable::AddSportman(CSportMan &sportman)
{
    if (m_sportmans.size() >= MAX_SPORTSMAN)
    {
        QMessageBox::warning(nullptr, QStringLiteral("报名限制"),
                             QStringLiteral("运动员人数已达上限（%1人），无法继续报名！").arg(MAX_SPORTSMAN));
        return false;
    }

    // 编号唯一性检查，目前是冗余校验，走不过来
    if (FindSportmanByNumber(sportman.m_number) >= 0)
    {
        QMessageBox::warning(nullptr, QStringLiteral("编号重复"),
                             QStringLiteral("运动员编号 %1 已存在，请使用其他编号！").arg(sportman.m_number));
        return false;
    }

    m_sportmans.push_back(sportman);
    m_number = m_sportmans.size();
    return true;
}

int SportsManInfoTable::FindSportmanByNumber(int number) const
{
    for (int i = 0; i < m_sportmans.size(); i++)
    {
        if (m_sportmans[i].m_number == number)
            return i;
    }
    return -1;
}

int SportsManInfoTable::GetSportsmanNum()
{
    m_number = m_sportmans.size();
    return m_number;
}

CSportMan & SportsManInfoTable::GetSportMan(int index)
{
    return m_sportmans[index];
}

void SportsManInfoTable::Clear()
{
    m_sportmans.clear();
    m_number = 0;
}