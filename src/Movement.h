#ifndef MOVEMENT_H
#define MOVEMENT_H

#include <QString>
#include <QDate>

class Movement {
public:
    enum Type { ReceiptType, WriteOffType, InventoryType };

    Movement();
    Movement(const QDate &date, Type type, const QString &material,
             double quantity, const QString &reference);

    QDate date() const;
    void setDate(const QDate &date);

    Type type() const;
    void setType(Type type);

    QString material() const;
    void setMaterial(const QString &material);

    double quantity() const;
    void setQuantity(double quantity);

    QString reference() const;
    void setReference(const QString &reference);

private:
    QDate m_date;
    Type m_type;
    QString m_material;
    double m_quantity;
    QString m_reference;
};

#endif // MOVEMENT_H
