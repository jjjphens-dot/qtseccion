#include "sortableheaderview.h"
#include <QPainter>
#include <QLinearGradient>
#include <QPolygonF>
#include <QPainterPath>

SortableHeaderView::SortableHeaderView(QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
{
    // paintSection绘制醒目箭头
    setSortIndicatorShown(false);
    setSectionsClickable(true);
    setSectionsMovable(false);
    setHighlightSections(true);
    setDefaultAlignment(Qt::AlignCenter);
    setMinimumSectionSize(56);
    // 限制任一列最大宽度
    setMaximumSectionSize(180);
}

void SortableHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid() || !model())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int sortCol = sortIndicatorSection();
    const bool isSortCol = (logicalIndex == sortCol);

    // 背景渐变
    QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
    if (isSortCol) {
        grad.setColorAt(0.0, QColor("#42A5F5"));
        grad.setColorAt(1.0, QColor("#1976D2"));
    } else {
        grad.setColorAt(0.0, QColor("#1E88E5"));
        grad.setColorAt(1.0, QColor("#1565C0"));
    }
    painter->fillRect(rect, grad);

    // 右分隔线
    painter->setPen(QPen(QColor(13, 71, 161, 140), 1));
    painter->drawLine(rect.topRight(), rect.bottomRight());

    // 表头文字
    QString text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
    painter->setPen(Qt::white);
    QFont f = this->font();
    f.setBold(true);
    painter->setFont(f);

    // 为箭头预留右侧空间
    const int arrowGap = isSortCol ? 20 : 6;
    const QRect textRect = rect.adjusted(6, 2, -arrowGap, -2);
    painter->drawText(textRect,
                      Qt::AlignVCenter | Qt::AlignHCenter | Qt::TextWordWrap,
                      text);

    // 排序箭头
    if (isSortCol) {
        const bool ascending = (sortIndicatorOrder() == Qt::AscendingOrder);
        drawSortArrow(painter, rect, ascending);
    }

    painter->restore();
}

void SortableHeaderView::drawSortArrow(QPainter *painter, const QRect &rect, bool ascending) const
{
    const qreal cx = rect.right() - 11;
    const qreal cy = rect.center().y();

    QPolygonF arrow;
    if (ascending) {
        arrow << QPointF(cx, cy - 5)
              << QPointF(cx - 6, cy + 4)
              << QPointF(cx + 6, cy + 4);
    } else {
        arrow << QPointF(cx, cy + 5)
              << QPointF(cx - 6, cy - 4)
              << QPointF(cx + 6, cy - 4);
    }


    painter->setBrush(QColor("#FFD54F"));
    painter->setPen(QPen(QColor(255, 255, 255, 220), 1));
    painter->drawPolygon(arrow);
}

QSize SortableHeaderView::sizeHint() const
{
    QSize s = QHeaderView::sizeHint();
    s.setHeight(46);
    return s;
}
