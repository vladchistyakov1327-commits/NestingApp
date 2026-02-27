#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>

// Simple singleton wrapper around QSqlDatabase to centralize connection logic
class DatabaseManager {
public:
    static DatabaseManager &instance();

    bool open(const QString &fileName);
    void close();

    QSqlDatabase database() const;

    /// create required tables if they do not exist
    bool createSchema();

private:
    DatabaseManager();
    ~DatabaseManager();

    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
