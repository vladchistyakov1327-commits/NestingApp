#ifndef MATERIALDELEGATE_H
#define MATERIALDELEGATE_H

#include <QStyledItemDelegate>
#include <QModelIndex>
#include <QPainter>

// Custom delegate to highlight rows with low stock (current < minimum)
class MaterialDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    MaterialDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    bool isLowStock(const QModelIndex &index) const;
};

#endif // MATERIALDELEGATE_H
