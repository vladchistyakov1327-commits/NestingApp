#include "Norm.h"

Norm::Norm()
    : m_quantity(0) {}

Norm::Norm(const QString &product, const QString &material, double quantity)
    : m_product(product), m_material(material), m_quantity(quantity) {}

QString Norm::product() const { return m_product; }
void Norm::setProduct(const QString &product) { m_product = product; }

QString Norm::material() const { return m_material; }
void Norm::setMaterial(const QString &material) { m_material = material; }

double Norm::quantity() const { return m_quantity; }
void Norm::setQuantity(double quantity) { m_quantity = quantity; }
