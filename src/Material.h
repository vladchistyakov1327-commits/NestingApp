#ifndef MATERIAL_H
#define MATERIAL_H

#include <QString>

// Represents a single material in the inventory
class Material {
public:
    Material();
    Material(const QString &name, const QString &unit, double price, double current,
             double minimum, const QString &supplier = QString());

    QString name() const;
    void setName(const QString &name);

    QString unit() const;
    void setUnit(const QString &unit);

    double price() const;
    void setPrice(double price);

    double current() const;
    void setCurrent(double current);

    double minimum() const;
    void setMinimum(double minimum);

    QString supplier() const;
    void setSupplier(const QString &supplier);

    bool isBelowMinimum() const;

private:
    QString m_name;
    QString m_unit;
    double m_price;
    double m_current;
    double m_minimum;
    QString m_supplier;
};

#endif // MATERIAL_H
