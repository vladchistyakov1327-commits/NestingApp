#include "MaterialDelegate.h"
#include <QAbstractItemModel>
#include <QStyleOptionViewItem>
#include <QColor>
#include <QBrush>

MaterialDelegate::MaterialDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {
}

bool MaterialDelegate::isLowStock(const QModelIndex &index) const {
    // assume parent row contains material data
    // column 4 = 'current', column 5 = 'minimum'
    const QAbstractItemModel *model = index.model();
    int row = index.row();
    
    double current = model->data(model->index(row, 4)).toDouble();
    double minimum = model->data(model->index(row, 5)).toDouble();
    
    return current < minimum;
}

void MaterialDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {
    QStyleOptionViewItem opt = option;
    
    // check if this row's stock is below minimum
    if (isLowStock(index)) {
        // highlight entire row in light red
        opt.backgroundBrush = QBrush(QColor(255, 200, 200)); // light red
    }
    
    QStyledItemDelegate::paint(painter, opt, index);
}
