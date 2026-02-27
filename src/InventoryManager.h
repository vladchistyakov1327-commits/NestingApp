#ifndef INVENTORYMANAGER_H
#define INVENTORYMANAGER_H

#include <QString>
#include <QDate>

class InventoryManager {
public:
    // add incoming materials, update stock and record movement
    static bool addReceipt(const QDate &date,
                           const QString &material,
                           double qty,
                           double price,
                           const QString &supplier = QString());

    // when an order with product and quantity is shipped, perform write-off
    // `orderId` is reference for journal
    static bool writeOffForShipment(const QString &product,
                                    double productQty,
                                    const QString &orderId);
};

#endif // INVENTORYMANAGER_H
