#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseService final
{
public:
    DatabaseService();
    ~DatabaseService();

    DatabaseService(const DatabaseService&) = delete;
    DatabaseService& operator=(const DatabaseService&) = delete;

    [[nodiscard]] bool initialize(QString* error);
    [[nodiscard]] QSqlDatabase database() const;

private:
    static constexpr auto ConnectionName = "notera-library";
    bool applyMigrations(QString* error);
    QSqlDatabase m_database;
};
