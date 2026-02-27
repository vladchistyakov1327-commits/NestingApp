#include "Movement.h"

Movement::Movement()
    : m_type(ReceiptType), m_quantity(0) {}

Movement::Movement(const QDate &date, Type type, const QString &material,
                   double quantity, const QString &reference)
    : m_date(date), m_type(type), m_material(material),
      m_quantity(quantity), m_reference(reference) {}

QDate Movement::date() const { return m_date; }
void Movement::setDate(const QDate &date) { m_date = date; }

Movement::Type Movement::type() const { return m_type; }
void Movement::setType(Type type) { m_type = type; }

QString Movement::material() const { return m_material; }
void Movement::setMaterial(const QString &material) { m_material = material; }

double Movement::quantity() const { return m_quantity; }
void Movement::setQuantity(double quantity) { m_quantity = quantity; }

QString Movement::reference() const { return m_reference; }
void Movement::setReference(const QString &reference) { m_reference = reference; }
