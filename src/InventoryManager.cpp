#include "InventoryManager.h"
#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

bool InventoryManager::addReceipt(const QDate &date,
                                   const QString &material,
                                   double qty,
                                   double price,
                                   const QString &supplier) {
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    // insert into receipts table
    query.prepare("INSERT INTO receipts(date, material, quantity, price, supplier) "
                  "VALUES(:date,:mat,:qty,:price,:sup)");
    query.bindValue(":date", date);
    query.bindValue(":mat", material);
    query.bindValue(":qty", qty);
    query.bindValue(":price", price);
    query.bindValue(":sup", supplier);
    if (!query.exec()) {
        qDebug() << "Failed to insert receipt:" << query.lastError().text();
        return false;
    }

    // update material stock
    QSqlQuery upd(db);
    upd.prepare("UPDATE materials SET current = current + :inc WHERE name = :mat");
    upd.bindValue(":inc", qty);
    upd.bindValue(":mat", material);
    if (!upd.exec()) {
        qDebug() << "Failed to update stock:" << upd.lastError().text();
        return false;
    }

    // record movement
    QSqlQuery mov(db);
    mov.prepare("INSERT INTO movements(date,type,material,quantity,reference) "
                "VALUES(:date,0,:mat,:qty,:ref)");
    mov.bindValue(":date", date);
    mov.bindValue(":mat", material);
    mov.bindValue(":qty", qty);
    mov.bindValue(":ref", tr("Receipt #%1").arg(query.lastInsertId().toInt()));
    if (!mov.exec()) {
        qDebug() << "Failed to record movement:" << mov.lastError().text();
        // not fatal
    }
    return true;
}

bool InventoryManager::writeOffForShipment(const QString &product,
                                           double productQty,
                                           const QString &orderId) {
    QSqlDatabase db = DatabaseManager::instance().database();
    if (!db.isOpen()) return false;

    // find norms for the product
    QSqlQuery normQuery(db);
    normQuery.prepare("SELECT material, quantity FROM norms WHERE product = :prod");
    normQuery.bindValue(":prod", product);
    if (!normQuery.exec()) {
        qDebug() << "Failed to query norms:" << normQuery.lastError().text();
        return false;
    }

    bool ok = true;
    while (normQuery.next()) {
        QString mat = normQuery.value(0).toString();
        double perUnit = normQuery.value(1).toDouble();
        double deduct = perUnit * productQty;

        // decrease stock
        QSqlQuery upd(db);
        upd.prepare("UPDATE materials SET current = current - :dec WHERE name = :mat");
        upd.bindValue(":dec", deduct);
        upd.bindValue(":mat", mat);
        if (!upd.exec()) {
            qDebug() << "Failed to decrement stock for" << mat << upd.lastError().text();
            ok = false;
            continue;
        }

        // log movement
        QSqlQuery mov(db);
        mov.prepare("INSERT INTO movements(date,type,material,quantity,reference) "
                    "VALUES(:date,1,:mat,:qty,:ref)");
        mov.bindValue(":date", QDate::currentDate());
        mov.bindValue(":mat", mat);
        mov.bindValue(":qty", -deduct);
        mov.bindValue(":ref", orderId);
        if (!mov.exec()) {
            qDebug() << "Failed to record write-off for" << mat << mov.lastError().text();
        }
    }
    return ok;
}
