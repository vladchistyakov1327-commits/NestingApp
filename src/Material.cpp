#include "Material.h"

Material::Material()
    : m_price(0), m_current(0), m_minimum(0) {}

Material::Material(const QString &name, const QString &unit, double price, double current,
                   double minimum, const QString &supplier)
    : m_name(name), m_unit(unit), m_price(price), m_current(current),
      m_minimum(minimum), m_supplier(supplier) {}

QString Material::name() const { return m_name; }
void Material::setName(const QString &name) { m_name = name; }

QString Material::unit() const { return m_unit; }
void Material::setUnit(const QString &unit) { m_unit = unit; }

double Material::price() const { return m_price; }
void Material::setPrice(double price) { m_price = price; }

double Material::current() const { return m_current; }
void Material::setCurrent(double current) { m_current = current; }

double Material::minimum() const { return m_minimum; }
void Material::setMinimum(double minimum) { m_minimum = minimum; }

QString Material::supplier() const { return m_supplier; }
void Material::setSupplier(const QString &supplier) { m_supplier = supplier; }

bool Material::isBelowMinimum() const {
    return m_current < m_minimum;
}
