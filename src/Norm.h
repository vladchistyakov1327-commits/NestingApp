#ifndef NORM_H
#define NORM_H

#include <QString>

// Represents consumption norm of a material for a specific product
class Norm {
public:
    Norm();
    Norm(const QString &product, const QString &material, double quantity);

    QString product() const;
    void setProduct(const QString &product);

    QString material() const;
    void setMaterial(const QString &material);

    double quantity() const;
    void setQuantity(double quantity);

private:
    QString m_product;
    QString m_material;
    double m_quantity;
};

#endif // NORM_H
