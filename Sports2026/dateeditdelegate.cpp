#include "dateeditdelegate.h"
#include <QDateEdit>

DateEditDelegate::DateEditDelegate(const QString &format, const QDate &minDate,
                                   const QDate &maxDate, QWidget *parent)
    : QItemDelegate(parent)
    , m_format(format)
    , m_minDate(minDate)
    , m_maxDate(maxDate)
{
}

QWidget *DateEditDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    QDateEdit *editor = new QDateEdit(parent);
    editor->setCalendarPopup(true);
    editor->setDisplayFormat(m_format);
    if (m_minDate.isValid())
        editor->setMinimumDate(m_minDate);
    if (m_maxDate.isValid())
        editor->setMaximumDate(m_maxDate);
    return editor;
}

void DateEditDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QDateEdit *de = qobject_cast<QDateEdit*>(editor);
    if (!de)
        return;

    QDate d = index.data(Qt::UserRole).toDate();
    if (!d.isValid())
        d = QDate::fromString(index.data(Qt::EditRole).toString(), m_format);
    if (!d.isValid())
        d = QDate::currentDate();

    // 时间范围检查
    if (m_minDate.isValid() && d < m_minDate)
        d = m_minDate;
    if (m_maxDate.isValid() && d > m_maxDate)
        d = m_maxDate;

    de->setDate(d);
}

void DateEditDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                    const QModelIndex &index) const
{
    QDateEdit *de = qobject_cast<QDateEdit*>(editor);
    if (!de)
        return;

    QDate d = de->date();
    model->setData(index, d.toString(m_format), Qt::EditRole);
    model->setData(index, d, Qt::UserRole);
}
