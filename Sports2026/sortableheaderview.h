#ifndef SORTABLEHEADERVIEW_H
#define SORTABLEHEADERVIEW_H

#include <QHeaderView>

/* 自定义水平表头视图 */
class SortableHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit SortableHeaderView(QWidget *parent = nullptr);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    QSize sizeHint() const override;

private:
    void drawSortArrow(QPainter *painter, const QRect &rect, bool ascending) const;
};

#endif // SORTABLEHEADERVIEW_H
