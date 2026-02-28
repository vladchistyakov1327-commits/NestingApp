#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

DatabaseManager::DatabaseManager() {
}

DatabaseManager::~DatabaseManager() {
    close();
}

DatabaseManager &DatabaseManager::instance() {
    static DatabaseManager mgr;
    return mgr;
}

bool DatabaseManager::open(const QString &fileName) {
    if (m_db.isOpen()) return true;
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(fileName);
    if (!m_db.open()) {
        qDebug() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }
    // ensure tables exist
    createSchema();
    return true;
}

bool DatabaseManager::createSchema() {
    if (!m_db.isOpen()) return false;
    QSqlQuery query(m_db);
    const char *sql[] = {
        // Inventory: materials and norms
        "CREATE TABLE IF NOT EXISTS materials("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE,"
        "unit TEXT,"
        "price REAL,"
        "current REAL,"
        "minimum REAL,"
        "supplier TEXT)",
        
        // Products/references
        "CREATE TABLE IF NOT EXISTS products("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE,"
        "description TEXT,"
        "price REAL,"
        "production_cost REAL)",
        
        "CREATE TABLE IF NOT EXISTS norms("
        "id INTEGER PRIMARY KEY,"
        "product TEXT,"
        "material TEXT,"
        "quantity REAL)",
        
        // Stock movements
        "CREATE TABLE IF NOT EXISTS receipts("
        "id INTEGER PRIMARY KEY,"
        "date DATE,"
        "material TEXT,"
        "quantity REAL,"
        "price REAL,"
        "supplier TEXT)",
        
        "CREATE TABLE IF NOT EXISTS movements("
        "id INTEGER PRIMARY KEY,"
        "date DATE,"
        "type INTEGER,"
        "material TEXT,"
        "quantity REAL,"
        "reference TEXT)",
        
        // Shipments
        "CREATE TABLE IF NOT EXISTS shipments("
        "id INTEGER PRIMARY KEY,"
        "date DATE,"
        "product TEXT,"
        "quantity REAL,"
        "customer TEXT,"
        "status TEXT)",
        
        // Employees and payroll
        "CREATE TABLE IF NOT EXISTS employees("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE,"
        "position TEXT,"
        "birth_date DATE,"
        "hire_date DATE,"
        "monthly_plan REAL,"
        "salary_type TEXT)",
        
        "CREATE TABLE IF NOT EXISTS salary("
        "id INTEGER PRIMARY KEY,"
        "employee_id INTEGER,"
        "month DATE,"
        "planned REAL,"
        "actual REAL,"
        "paid INTEGER,"
        "difference REAL)",
        
        "CREATE TABLE IF NOT EXISTS performed("
        "id INTEGER PRIMARY KEY,"
        "date DATE,"
        "employee_id INTEGER,"
        "operation TEXT,"
        "fact REAL,"
        "control REAL)",
        
        // Notifications
        "CREATE TABLE IF NOT EXISTS notifications("
        "id INTEGER PRIMARY KEY,"
        "type INTEGER,"
        "message TEXT,"
        "priority INTEGER,"
        "created_at DATETIME,"
        "read_at DATETIME)",
        
        // Employee achievements
        "CREATE TABLE IF NOT EXISTS achievements("
        "id INTEGER PRIMARY KEY,"
        "employee_id INTEGER,"
        "badge_name TEXT,"
        "earned_date DATE)"
    };
    
    for (auto stmt : sql) {
        if (!query.exec(stmt)) {
            qDebug() << "Schema creation failed:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

void DatabaseManager::close() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

QSqlDatabase DatabaseManager::database() const {
    return m_db;
}
