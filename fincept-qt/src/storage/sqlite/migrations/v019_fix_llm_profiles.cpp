// v019_fix_llm_profiles.cpp

#include "storage/sqlite/migrations/MigrationRunner.h"
#include <QSqlError>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> sql_v019(QSqlDatabase& db, const char* stmt) {
    QSqlQuery q(db);
    if (!q.exec(stmt))
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<void> apply_v019(QSqlDatabase& db) {
    // We recreate the table if it was dropped or if the user is on an old schema.
    auto r = sql_v019(db,
                      "CREATE TABLE IF NOT EXISTS llm_profiles ("
                      "  id               TEXT    PRIMARY KEY,"
                      "  name             TEXT    NOT NULL UNIQUE,"
                      "  provider         TEXT    NOT NULL,"
                      "  model_id         TEXT    NOT NULL,"
                      "  api_key          TEXT    NOT NULL DEFAULT '',"
                      "  base_url         TEXT    NOT NULL DEFAULT '',"
                      "  temperature      REAL    NOT NULL DEFAULT 0.7,"
                      "  max_tokens       INTEGER NOT NULL DEFAULT 4096,"
                      "  system_prompt    TEXT    NOT NULL DEFAULT '',"
                      "  is_default       INTEGER NOT NULL DEFAULT 0,"
                      "  created_at       TEXT    DEFAULT (datetime('now')),"
                      "  updated_at       TEXT    DEFAULT (datetime('now'))"
                      ")");
    if (r.is_err()) return r;

    // llm_profile_assignments
    r = sql_v019(db,
                 "CREATE TABLE IF NOT EXISTS llm_profile_assignments ("
                 "  id               INTEGER PRIMARY KEY AUTOINCREMENT,"
                 "  context_type     TEXT    NOT NULL,"
                 "  context_id       TEXT,"
                 "  profile_id       TEXT    NOT NULL"
                 "    REFERENCES llm_profiles(id) ON DELETE CASCADE,"
                 "  created_at       TEXT    DEFAULT (datetime('now')),"
                 "  updated_at       TEXT    DEFAULT (datetime('now')),"
                 "  UNIQUE(context_type, context_id)"
                 ")");
    if (r.is_err()) return r;

    // Ensure all columns exist in case the table exists but is old
    QSqlQuery q(db);
    q.exec("ALTER TABLE llm_profiles ADD COLUMN api_key TEXT NOT NULL DEFAULT ''");
    q.exec("ALTER TABLE llm_profiles ADD COLUMN base_url TEXT NOT NULL DEFAULT ''");
    q.exec("ALTER TABLE llm_profiles ADD COLUMN system_prompt TEXT NOT NULL DEFAULT ''");
    q.exec("ALTER TABLE llm_profiles ADD COLUMN updated_at TEXT DEFAULT (datetime('now'))");

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v019() {
    static bool done = false;
    if (done) return;
    done = true;
    MigrationRunner::register_migration({19, "fix_llm_profiles", apply_v019});
}

} // namespace fincept
