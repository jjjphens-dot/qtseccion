#ifndef DATEEDITDELEGATE_H
#define DATEEDITDELEGATE_H

#include <QItemDelegate>
#include <QDate>
#include <QString>

// 日期编辑委托
class DateEditDelegate : public QItemDelegate
{
public:
    DateEditDelegate(const QString &format = QStringLiteral("yyyy-MM"),
                     const QDate &minDate = QDate(),
                     const QDate &maxDate = QDate(),
                     QWidget *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

private:
    QString m_format;
    QDate m_minDate;
    QDate m_maxDate;
};

#endif // DATEEDITDELEGATE_H
