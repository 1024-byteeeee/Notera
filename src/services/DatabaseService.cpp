#include "services/DatabaseService.h"

#include <QSqlError>
#include <QSqlQuery>

#include "platform/AppDataPaths.h"

DatabaseService::DatabaseService() = default;

DatabaseService::~DatabaseService()
{
    const auto name = m_database.connectionName();
    m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(name);
}

bool DatabaseService::initialize(QString* error)
{
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QString::fromLatin1(ConnectionName));
    m_database.setDatabaseName(AppDataPaths::databaseDirectory() + QStringLiteral("/notera.db"));
    if (!m_database.open()) {
        *error = QStringLiteral("Notera 无法打开乐谱库数据库：%1").arg(m_database.lastError().text());
        return false;
    }

    return applyMigrations(error);
}

QSqlDatabase DatabaseService::database() const
{
    return m_database;
}

bool DatabaseService::applyMigrations(QString* error)
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON")) || !query.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        *error = query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        *error = query.lastError().text();
        return false;
    }

    if (query.value(0).toInt() >= 1) {
        return true;
    }

    if (!m_database.transaction()) {
        *error = m_database.lastError().text();
        return false;
    }

    const auto succeeded = query.exec(QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS scores (
            id TEXT PRIMARY KEY,
            title TEXT NOT NULL,
            composer TEXT,
            file_name TEXT NOT NULL,
            file_path TEXT NOT NULL,
            file_type TEXT NOT NULL,
            page_count INTEGER NOT NULL DEFAULT 1,
            thumbnail_path TEXT,
            favorite INTEGER NOT NULL DEFAULT 0,
            last_page INTEGER NOT NULL DEFAULT 1,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            last_opened_at INTEGER
        )
    )"))
        && query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS tags (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE)"))
        && query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS score_tags (score_id TEXT NOT NULL, tag_id TEXT NOT NULL, PRIMARY KEY (score_id, tag_id), FOREIGN KEY(score_id) REFERENCES scores(id) ON DELETE CASCADE, FOREIGN KEY(tag_id) REFERENCES tags(id) ON DELETE CASCADE)"))
        && query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS annotations (id TEXT PRIMARY KEY, score_id TEXT NOT NULL, page INTEGER NOT NULL, type TEXT NOT NULL, data TEXT NOT NULL, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL, FOREIGN KEY(score_id) REFERENCES scores(id) ON DELETE CASCADE)"))
        && query.exec(QStringLiteral("PRAGMA user_version = 1"));

    if (!succeeded) {
        m_database.rollback();
        *error = query.lastError().text();
        return false;
    }
    if (!m_database.commit()) {
        *error = m_database.lastError().text();
        return false;
    }
    return true;
}
