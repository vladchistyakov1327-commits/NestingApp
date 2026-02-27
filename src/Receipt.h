#ifndef RECEIPT_H
#define RECEIPT_H

#include <QString>
#include <QDate>

// Record of material receipt into stock
class Receipt {
public:
    Receipt();
    Receipt(const QDate &date, const QString &material, double quantity,
            double price, const QString &supplier = QString());

    QDate date() const;
    void setDate(const QDate &date);

    QString material() const;
    void setMaterial(const QString &material);

    double quantity() const;
    void setQuantity(double quantity);

    double price() const;

    double total() const; // quantity * price

    QString supplier() const;
    void setSupplier(const QString &supplier);

private:
    QDate m_date;
    QString m_material;
    double m_quantity;
    double m_price;
    QString m_supplier;
};

#endif // RECEIPT_H
