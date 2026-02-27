#include "Receipt.h"

Receipt::Receipt()
    : m_quantity(0), m_price(0) {}

Receipt::Receipt(const QDate &date, const QString &material, double quantity,
                 double price, const QString &supplier)
    : m_date(date), m_material(material), m_quantity(quantity),
      m_price(price), m_supplier(supplier) {}

QDate Receipt::date() const { return m_date; }
void Receipt::setDate(const QDate &date) { m_date = date; }

QString Receipt::material() const { return m_material; }
void Receipt::setMaterial(const QString &material) { m_material = material; }

double Receipt::quantity() const { return m_quantity; }
void Receipt::setQuantity(double quantity) { m_quantity = quantity; }

double Receipt::price() const { return m_price; }

double Receipt::total() const { return m_quantity * m_price; }

QString Receipt::supplier() const { return m_supplier; }
void Receipt::setSupplier(const QString &supplier) { m_supplier = supplier; }
