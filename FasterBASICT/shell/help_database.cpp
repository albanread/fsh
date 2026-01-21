//
// help_database.cpp
// FasterBASIC Shell - SQLite-Based Help Database Implementation
//
// Provides fast, searchable help documentation using SQLite with FTS5.
//

#include "help_database.h"
#include "../src/modular_commands.h"
#include "../src/command_registry_core.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace FasterBASIC {
namespace HelpSystem {

// =============================================================================
// SQL Schema Constants
// =============================================================================

static const char* SCHEMA_VERSION = "1";

static const char* SQL_CREATE_COMMANDS = R"(
CREATE TABLE IF NOT EXISTS commands (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE COLLATE NOCASE,
    type TEXT NOT NULL,
    category TEXT NOT NULL,
    description TEXT NOT NULL,
    lua_function TEXT NOT NULL,
    return_type TEXT,
    requires_parens INTEGER DEFAULT 0,
    usage TEXT,
    example_code TEXT,
    notes TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_commands_category ON commands(category);
CREATE INDEX IF NOT EXISTS idx_commands_type ON commands(type);
CREATE INDEX IF NOT EXISTS idx_commands_name ON commands(name COLLATE NOCASE);
)";

static const char* SQL_CREATE_PARAMETERS = R"(
CREATE TABLE IF NOT EXISTS parameters (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    command_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    name TEXT NOT NULL,
    type TEXT NOT NULL,
    description TEXT,
    is_optional INTEGER DEFAULT 0,
    default_value TEXT,
    FOREIGN KEY (command_id) REFERENCES commands(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_parameters_command ON parameters(command_id);
)";

static const char* SQL_CREATE_ARTICLES = R"(
CREATE TABLE IF NOT EXISTS articles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE COLLATE NOCASE,
    title TEXT NOT NULL,
    category TEXT NOT NULL,
    content TEXT NOT NULL,
    author TEXT,
    difficulty TEXT,
    estimated_time INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_articles_category ON articles(category);
CREATE INDEX IF NOT EXISTS idx_articles_difficulty ON articles(difficulty);
CREATE INDEX IF NOT EXISTS idx_articles_name ON articles(name COLLATE NOCASE);
)";

static const char* SQL_CREATE_TAGS = R"(
CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE COLLATE NOCASE
);

CREATE INDEX IF NOT EXISTS idx_tags_name ON tags(name COLLATE NOCASE);
)";

static const char* SQL_CREATE_COMMAND_TAGS = R"(
CREATE TABLE IF NOT EXISTS command_tags (
    command_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (command_id, tag_id),
    FOREIGN KEY (command_id) REFERENCES commands(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_command_tags_command ON command_tags(command_id);
CREATE INDEX IF NOT EXISTS idx_command_tags_tag ON command_tags(tag_id);
)";

static const char* SQL_CREATE_ARTICLE_TAGS = R"(
CREATE TABLE IF NOT EXISTS article_tags (
    article_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (article_id, tag_id),
    FOREIGN KEY (article_id) REFERENCES articles(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_article_tags_article ON article_tags(article_id);
CREATE INDEX IF NOT EXISTS idx_article_tags_tag ON article_tags(tag_id);
)";

static const char* SQL_CREATE_RELATED_COMMANDS = R"(
CREATE TABLE IF NOT EXISTS related_commands (
    command_id INTEGER NOT NULL,
    related_command_id INTEGER NOT NULL,
    PRIMARY KEY (command_id, related_command_id),
    FOREIGN KEY (command_id) REFERENCES commands(id) ON DELETE CASCADE,
    FOREIGN KEY (related_command_id) REFERENCES commands(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_related_commands_command ON related_commands(command_id);
)";

static const char* SQL_CREATE_ARTICLE_COMMANDS = R"(
CREATE TABLE IF NOT EXISTS article_commands (
    article_id INTEGER NOT NULL,
    command_id INTEGER NOT NULL,
    PRIMARY KEY (article_id, command_id),
    FOREIGN KEY (article_id) REFERENCES articles(id) ON DELETE CASCADE,
    FOREIGN KEY (command_id) REFERENCES commands(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_article_commands_article ON article_commands(article_id);
CREATE INDEX IF NOT EXISTS idx_article_commands_command ON article_commands(command_id);
)";

static const char* SQL_CREATE_METADATA = R"(
CREATE TABLE IF NOT EXISTS metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
)";

static const char* SQL_CREATE_COMMANDS_FTS = R"(
CREATE VIRTUAL TABLE IF NOT EXISTS commands_fts USING fts5(
    command_id UNINDEXED,
    name,
    description,
    category,
    usage,
    example_code,
    notes,
    tokenize = 'porter unicode61'
);
)";

static const char* SQL_CREATE_ARTICLES_FTS = R"(
CREATE VIRTUAL TABLE IF NOT EXISTS articles_fts USING fts5(
    article_id UNINDEXED,
    name,
    title,
    category,
    content,
    tokenize = 'porter unicode61'
);
)";

// FTS triggers - keep in sync with main tables
static const char* SQL_CREATE_FTS_TRIGGERS = R"(
CREATE TRIGGER IF NOT EXISTS commands_fts_insert AFTER INSERT ON commands
BEGIN
    INSERT INTO commands_fts(command_id, name, description, category, usage, example_code, notes)
    VALUES (NEW.id, NEW.name, NEW.description, NEW.category, NEW.usage, NEW.example_code, NEW.notes);
END;

CREATE TRIGGER IF NOT EXISTS commands_fts_update AFTER UPDATE ON commands
BEGIN
    UPDATE commands_fts 
    SET name = NEW.name,
        description = NEW.description,
        category = NEW.category,
        usage = NEW.usage,
        example_code = NEW.example_code,
        notes = NEW.notes
    WHERE command_id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS commands_fts_delete AFTER DELETE ON commands
BEGIN
    DELETE FROM commands_fts WHERE command_id = OLD.id;
END;

CREATE TRIGGER IF NOT EXISTS articles_fts_insert AFTER INSERT ON articles
BEGIN
    INSERT INTO articles_fts(article_id, name, title, category, content)
    VALUES (NEW.id, NEW.name, NEW.title, NEW.category, NEW.content);
END;

CREATE TRIGGER IF NOT EXISTS articles_fts_update AFTER UPDATE ON articles
BEGIN
    UPDATE articles_fts 
    SET name = NEW.name,
        title = NEW.title,
        category = NEW.category,
        content = NEW.content
    WHERE article_id = NEW.id;
END;

CREATE TRIGGER IF NOT EXISTS articles_fts_delete AFTER DELETE ON articles
BEGIN
    DELETE FROM articles_fts WHERE article_id = OLD.id;
END;
)";

// =============================================================================
// HelpDatabase Implementation
// =============================================================================

HelpDatabase::HelpDatabase() 
    : m_db(nullptr)
    , m_isOpen(false)
    , m_progressCallback(nullptr) {
}

HelpDatabase::~HelpDatabase() {
    close();
}

bool HelpDatabase::open(const std::string& dbPath) {
    if (m_isOpen) {
        close();
    }
    
    clearError();
    
    // Ensure parent directory exists
    size_t lastSlash = dbPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        std::string dirPath = dbPath.substr(0, lastSlash);
        
        // Create directory recursively on macOS/Linux
        std::string mkdirCmd = "mkdir -p \"" + dirPath + "\" 2>/dev/null";
        system(mkdirCmd.c_str());
    }
    
    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        setError("Failed to open database: " + std::string(sqlite3_errmsg(m_db)));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }
    
    m_isOpen = true;
    m_dbPath = dbPath;
    
    // Enable foreign keys
    execute("PRAGMA foreign_keys = ON;");
    
    // Check if schema exists
    if (!schemaExists()) {
        // Create schema on first open
        if (!createSchema()) {
            setError("Failed to create database schema");
            close();
            return false;
        }
    }
    
    return true;
}

void HelpDatabase::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_isOpen = false;
    m_dbPath.clear();
}

bool HelpDatabase::isOpen() const {
    return m_isOpen;
}

std::string HelpDatabase::getDatabasePath() const {
    return m_dbPath;
}

std::string HelpDatabase::getDefaultDatabasePath() {
#ifdef __APPLE__
    // macOS: ~/Library/Application Support/FasterBASIC/help.db
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/Library/Application Support/FasterBASIC/help.db";
    }
#elif defined(__linux__)
    // Linux: ~/.local/share/fasterbasic/help.db
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.local/share/fasterbasic/help.db";
    }
#elif defined(_WIN32)
    // Windows: %APPDATA%\FasterBASIC\help.db
    const char* appdata = getenv("APPDATA");
    if (appdata) {
        return std::string(appdata) + "\\FasterBASIC\\help.db";
    }
#endif
    
    // Fallback
    return "./help.db";
}

bool HelpDatabase::rebuild() {
    if (!m_isOpen) {
        setError("Database not open");
        return false;
    }
    
    clearError();
    reportProgress("Starting rebuild...", 0, 100);
    
    // Drop all tables
    reportProgress("Dropping old tables...", 10, 100);
    if (!dropAllTables()) {
        setError("Failed to drop tables");
        return false;
    }
    
    // Create schema
    reportProgress("Creating schema...", 20, 100);
    if (!createSchema()) {
        setError("Failed to create schema");
        return false;
    }
    
    // Populate from registry
    reportProgress("Loading commands from registry...", 40, 100);
    if (!populateFromRegistry()) {
        setError("Failed to populate from command registry");
        return false;
    }
    
    // Populate from articles
    reportProgress("Loading articles...", 70, 100);
    if (!populateFromArticles()) {
        // This is not fatal - articles are optional
        std::cerr << "Warning: Failed to load articles\n";
    }
    
    // Update metadata
    reportProgress("Updating metadata...", 90, 100);
    updateMetadata("schema_version", SCHEMA_VERSION);
    updateMetadata("last_rebuild", "datetime('now')");
    
    reportProgress("Rebuild complete!", 100, 100);
    
    return true;
}

bool HelpDatabase::needsRebuild() {
    if (!m_isOpen) {
        return true;
    }
    
    if (!schemaExists()) {
        return true;
    }
    
    // Check schema version
    std::string version = getDatabaseSchemaVersion();
    if (version != SCHEMA_VERSION) {
        return true;
    }
    
    // Check if database is empty
    int count = getCommandCount();
    if (count == 0) {
        return true;
    }
    
    return false;
}

void HelpDatabase::setRebuildProgressCallback(RebuildProgressCallback callback) {
    m_progressCallback = callback;
}

// =============================================================================
// Schema Management
// =============================================================================

bool HelpDatabase::createSchema() {
    if (!beginTransaction()) {
        return false;
    }
    
    // Create tables
    if (!execute(SQL_CREATE_COMMANDS)) return false;
    if (!execute(SQL_CREATE_PARAMETERS)) return false;
    if (!execute(SQL_CREATE_ARTICLES)) return false;
    if (!execute(SQL_CREATE_TAGS)) return false;
    if (!execute(SQL_CREATE_COMMAND_TAGS)) return false;
    if (!execute(SQL_CREATE_ARTICLE_TAGS)) return false;
    if (!execute(SQL_CREATE_RELATED_COMMANDS)) return false;
    if (!execute(SQL_CREATE_ARTICLE_COMMANDS)) return false;
    if (!execute(SQL_CREATE_METADATA)) return false;
    if (!execute(SQL_CREATE_COMMANDS_FTS)) return false;
    if (!execute(SQL_CREATE_ARTICLES_FTS)) return false;
    if (!execute(SQL_CREATE_FTS_TRIGGERS)) return false;
    
    // Initialize metadata
    std::string sql = "INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)";
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (stmt) {
        bindText(stmt, 1, "schema_version");
        bindText(stmt, 2, SCHEMA_VERSION);
        sqlite3_step(stmt);
        finalizeStatement(stmt);
    }
    
    return commitTransaction();
}

bool HelpDatabase::dropAllTables() {
    const char* dropTables[] = {
        "DROP TABLE IF EXISTS commands_fts",
        "DROP TABLE IF EXISTS articles_fts",
        "DROP TABLE IF EXISTS article_commands",
        "DROP TABLE IF EXISTS related_commands",
        "DROP TABLE IF EXISTS article_tags",
        "DROP TABLE IF EXISTS command_tags",
        "DROP TABLE IF EXISTS tags",
        "DROP TABLE IF EXISTS parameters",
        "DROP TABLE IF EXISTS articles",
        "DROP TABLE IF EXISTS commands",
        "DROP TABLE IF EXISTS metadata",
        nullptr
    };
    
    for (int i = 0; dropTables[i] != nullptr; ++i) {
        if (!execute(dropTables[i])) {
            return false;
        }
    }
    
    return true;
}

bool HelpDatabase::schemaExists() {
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='commands'";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return false;
    }
    
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    finalizeStatement(stmt);
    
    return exists;
}

std::string HelpDatabase::getDatabaseSchemaVersion() {
    const char* sql = "SELECT value FROM metadata WHERE key = 'schema_version'";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return "";
    }
    
    std::string version;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) {
            version = text;
        }
    }
    
    finalizeStatement(stmt);
    return version;
}

bool HelpDatabase::updateMetadata(const std::string& key, const std::string& value) {
    const char* sql = "INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return false;
    }
    
    bindText(stmt, 1, key);
    bindText(stmt, 2, value);
    
    int rc = sqlite3_step(stmt);
    finalizeStatement(stmt);
    
    return (rc == SQLITE_DONE);
}

// =============================================================================
// Population from Command Registry
// =============================================================================

bool HelpDatabase::populateFromRegistry() {
    using namespace ModularCommands;
    
    auto& registry = getGlobalCommandRegistry();
    const auto& commands = registry.getAllCommands();
    
    if (!beginTransaction()) {
        return false;
    }
    
    int count = 0;
    int total = commands.size();
    
    for (const auto& pair : commands) {
        const CommandDefinition* cmd = &pair.second;
        
        if (!insertCommand(cmd)) {
            rollbackTransaction();
            return false;
        }
        
        count++;
        if (count % 50 == 0) {
            reportProgress("Loading commands...", 40 + (count * 30 / total), 100);
        }
    }
    
    return commitTransaction();
}

bool HelpDatabase::insertCommand(const ModularCommands::CommandDefinition* cmd) {
    const char* sql = R"(
        INSERT INTO commands (name, type, category, description, lua_function, 
                            return_type, requires_parens, usage, example_code, notes)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        return false;
    }
    
    bindText(stmt, 1, cmd->commandName);
    bindText(stmt, 2, cmd->isFunction ? "function" : "command");
    bindText(stmt, 3, cmd->category);
    bindText(stmt, 4, cmd->description);
    bindText(stmt, 5, cmd->luaFunction);
    bindText(stmt, 6, ModularCommands::returnTypeToString(cmd->returnType));
    bindInt(stmt, 7, cmd->requiresParentheses ? 1 : 0);
    bindText(stmt, 8, cmd->getUsage()); // Auto-generate or use custom usage
    bindText(stmt, 9, ""); // exampleCode - to be added later
    bindText(stmt, 10, ""); // notes - to be added later
    
    int rc = sqlite3_step(stmt);
    finalizeStatement(stmt);
    
    if (rc != SQLITE_DONE) {
        return false;
    }
    
    int commandId = sqlite3_last_insert_rowid(m_db);
    
    // Insert parameters
    if (!insertParameters(commandId, cmd)) {
        return false;
    }
    
    return true;
}

bool HelpDatabase::insertParameters(int commandId, const ModularCommands::CommandDefinition* cmd) {
    const char* sql = R"(
        INSERT INTO parameters (command_id, position, name, type, description, 
                              is_optional, default_value)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    for (size_t i = 0; i < cmd->parameters.size(); ++i) {
        const auto& param = cmd->parameters[i];
        
        sqlite3_stmt* stmt = prepareStatement(sql);
        if (!stmt) {
            return false;
        }
        
        bindInt(stmt, 1, commandId);
        bindInt(stmt, 2, (int)i);
        bindText(stmt, 3, param.name);
        bindText(stmt, 4, ModularCommands::parameterTypeToString(param.type));
        bindText(stmt, 5, param.description);
        bindInt(stmt, 6, param.isOptional ? 1 : 0);
        bindText(stmt, 7, param.defaultValue);
        
        int rc = sqlite3_step(stmt);
        finalizeStatement(stmt);
        
        if (rc != SQLITE_DONE) {
            return false;
        }
    }
    
    return true;
}

bool HelpDatabase::insertCommandTags(int commandId, const std::vector<std::string>& tags) {
    for (const auto& tag : tags) {
        int tagId = getOrCreateTag(tag);
        if (tagId < 0) {
            return false;
        }
        
        const char* sql = "INSERT OR IGNORE INTO command_tags (command_id, tag_id) VALUES (?, ?)";
        sqlite3_stmt* stmt = prepareStatement(sql);
        if (!stmt) {
            return false;
        }
        
        bindInt(stmt, 1, commandId);
        bindInt(stmt, 2, tagId);
        
        sqlite3_step(stmt);
        finalizeStatement(stmt);
    }
    
    return true;
}

int HelpDatabase::getOrCreateTag(const std::string& tagName) {
    // Try to get existing tag
    const char* selectSql = "SELECT id FROM tags WHERE name = ? COLLATE NOCASE";
    sqlite3_stmt* stmt = prepareStatement(selectSql);
    
    if (!stmt) {
        return -1;
    }
    
    bindText(stmt, 1, tagName);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        finalizeStatement(stmt);
        return id;
    }
    
    finalizeStatement(stmt);
    
    // Create new tag
    const char* insertSql = "INSERT INTO tags (name) VALUES (?)";
    stmt = prepareStatement(insertSql);
    
    if (!stmt) {
        return -1;
    }
    
    bindText(stmt, 1, tagName);
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        finalizeStatement(stmt);
        return -1;
    }
    
    int id = sqlite3_last_insert_rowid(m_db);
    finalizeStatement(stmt);
    
    return id;
}

int HelpDatabase::getCommandId(const std::string& commandName) {
    const char* sql = "SELECT id FROM commands WHERE name = ? COLLATE NOCASE";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return -1;
    }
    
    bindText(stmt, 1, commandName);
    
    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    
    finalizeStatement(stmt);
    return id;
}

// =============================================================================
// Population from Article Files
// =============================================================================

bool HelpDatabase::populateFromArticles() {
    // TODO: Load articles from filesystem
    // For now, just return true (articles are optional)
    return true;
}

std::vector<ArticleHelp> HelpDatabase::loadArticlesFromFiles() {
    std::vector<ArticleHelp> articles;
    // TODO: Implement article loading from markdown files
    return articles;
}

std::optional<ArticleHelp> HelpDatabase::parseArticleFile(const std::string& filePath) {
    // TODO: Implement markdown parsing with YAML frontmatter
    return std::nullopt;
}

bool HelpDatabase::insertArticle(const ArticleHelp& article) {
    // TODO: Implement article insertion
    return true;
}

// =============================================================================
// Search and Query
// =============================================================================

std::vector<SearchResult> HelpDatabase::search(const std::string& query, int limit) {
    std::vector<SearchResult> results;
    
    // Search commands
    auto cmdResults = searchCommands(query, limit);
    results.insert(results.end(), cmdResults.begin(), cmdResults.end());
    
    // Search articles
    auto artResults = searchArticles(query, limit);
    results.insert(results.end(), artResults.begin(), artResults.end());
    
    // Sort by relevance
    std::sort(results.begin(), results.end(), 
        [](const SearchResult& a, const SearchResult& b) {
            return a.relevance > b.relevance;
        });
    
    // Limit total results
    if ((int)results.size() > limit) {
        results.resize(limit);
    }
    
    return results;
}

std::vector<SearchResult> HelpDatabase::searchCommands(const std::string& query, int limit) {
    std::vector<SearchResult> results;
    
    // First, check for exact command name match (case-insensitive)
    const char* exactSql = R"(
        SELECT c.name, c.type, c.category, c.description, c.usage
        FROM commands c
        WHERE LOWER(c.name) = LOWER(?)
        LIMIT 1
    )";
    
    sqlite3_stmt* exactStmt = prepareStatement(exactSql);
    if (exactStmt) {
        bindText(exactStmt, 1, query);
        
        if (sqlite3_step(exactStmt) == SQLITE_ROW) {
            SearchResult result;
            result.name = (const char*)sqlite3_column_text(exactStmt, 0);
            result.type = (const char*)sqlite3_column_text(exactStmt, 1);
            result.category = (const char*)sqlite3_column_text(exactStmt, 2);
            result.description = (const char*)sqlite3_column_text(exactStmt, 3);
            const char* usage = (const char*)sqlite3_column_text(exactStmt, 4);
            if (usage) {
                result.excerpt = std::string("Usage: ") + usage;
            }
            result.relevance = 1.0f; // Highest relevance for exact match
            
            results.push_back(result);
        }
        
        finalizeStatement(exactStmt);
        
        // If we found an exact match and only want one result, return it
        if (!results.empty() && limit == 1) {
            return results;
        }
    }
    
    // Then do full-text search for partial matches
    const char* sql = R"(
        SELECT c.name, c.type, c.category, c.description, c.usage, rank * -1 as relevance
        FROM commands_fts 
        JOIN commands c ON commands_fts.command_id = c.id
        WHERE commands_fts MATCH ?
        ORDER BY rank
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        return results;
    }
    
    bindText(stmt, 1, query);
    bindInt(stmt, 2, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SearchResult result;
        result.name = (const char*)sqlite3_column_text(stmt, 0);
        result.type = (const char*)sqlite3_column_text(stmt, 1);
        result.category = (const char*)sqlite3_column_text(stmt, 2);
        result.description = (const char*)sqlite3_column_text(stmt, 3);
        const char* usage = (const char*)sqlite3_column_text(stmt, 4);
        if (usage) {
            result.excerpt = std::string("Usage: ") + usage;
        }
        result.relevance = (float)sqlite3_column_double(stmt, 5);
        
        // Skip if already added as exact match
        bool isDuplicate = false;
        for (const auto& existing : results) {
            if (existing.name == result.name) {
                isDuplicate = true;
                break;
            }
        }
        
        if (!isDuplicate) {
            results.push_back(result);
        }
        
        // Stop if we've reached the limit
        if ((int)results.size() >= limit) {
            break;
        }
    }
    
    finalizeStatement(stmt);
    return results;
}

std::vector<SearchResult> HelpDatabase::searchArticles(const std::string& query, int limit) {
    std::vector<SearchResult> results;
    
    const char* sql = R"(
        SELECT a.name, a.title, a.category, 
               snippet(articles_fts, 3, '<b>', '</b>', '...', 32) as excerpt,
               rank * -1 as relevance
        FROM articles_fts
        JOIN articles a ON articles_fts.article_id = a.id
        WHERE articles_fts MATCH ?
        ORDER BY rank
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        return results;
    }
    
    bindText(stmt, 1, query);
    bindInt(stmt, 2, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SearchResult result;
        result.name = (const char*)sqlite3_column_text(stmt, 1);
        result.type = "article";
        result.category = (const char*)sqlite3_column_text(stmt, 2);
        result.description = result.name;
        result.excerpt = (const char*)sqlite3_column_text(stmt, 3);
        result.relevance = (float)sqlite3_column_double(stmt, 4);
        
        results.push_back(result);
    }
    
    finalizeStatement(stmt);
    return results;
}

std::vector<SearchResult> HelpDatabase::searchByTag(const std::string& tag) {
    std::vector<SearchResult> results;
    
    const char* sql = R"(
        SELECT c.name, c.type, c.category, c.description
        FROM commands c
        JOIN command_tags ct ON c.id = ct.command_id
        JOIN tags t ON ct.tag_id = t.id
        WHERE t.name = ? COLLATE NOCASE
        ORDER BY c.name
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        return results;
    }
    
    bindText(stmt, 1, tag);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SearchResult result;
        result.name = (const char*)sqlite3_column_text(stmt, 0);
        result.type = (const char*)sqlite3_column_text(stmt, 1);
        result.category = (const char*)sqlite3_column_text(stmt, 2);
        result.description = (const char*)sqlite3_column_text(stmt, 3);
        result.relevance = 1.0f;
        
        results.push_back(result);
    }
    
    finalizeStatement(stmt);
    return results;
}

std::vector<std::string> HelpDatabase::findSimilarCommands(const std::string& partialName, int limit) {
    std::vector<std::string> results;
    
    const char* sql = R"(
        SELECT name
        FROM commands
        WHERE name LIKE '%' || ? || '%'
        ORDER BY 
            CASE 
                WHEN name = ? THEN 0
                WHEN name LIKE ? || '%' THEN 1
                WHEN name LIKE '%' || ? THEN 2
                ELSE 3
            END,
            name
        LIMIT ?
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        return results;
    }
    
    bindText(stmt, 1, partialName);
    bindText(stmt, 2, partialName);
    bindText(stmt, 3, partialName);
    bindText(stmt, 4, partialName);
    bindInt(stmt, 5, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back((const char*)sqlite3_column_text(stmt, 0));
    }
    
    finalizeStatement(stmt);
    return results;
}

// =============================================================================
// Command Documentation
// =============================================================================

std::unique_ptr<CommandHelp> HelpDatabase::getCommandHelp(const std::string& commandName) {
    const char* sql = R"(
        SELECT id, name, type, category, description, lua_function, 
               return_type, requires_parens, usage, example_code, notes
        FROM commands
        WHERE name = ? COLLATE NOCASE
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        return nullptr;
    }
    
    bindText(stmt, 1, commandName);
    
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        finalizeStatement(stmt);
        return nullptr;
    }
    
    auto help = std::make_unique<CommandHelp>();
    int commandId = sqlite3_column_int(stmt, 0);
    help->name = (const char*)sqlite3_column_text(stmt, 1);
    help->type = (const char*)sqlite3_column_text(stmt, 2);
    help->category = (const char*)sqlite3_column_text(stmt, 3);
    help->description = (const char*)sqlite3_column_text(stmt, 4);
    help->luaFunction = (const char*)sqlite3_column_text(stmt, 5);
    help->returnType = (const char*)sqlite3_column_text(stmt, 6);
    help->requiresParens = sqlite3_column_int(stmt, 7) != 0;
    
    // Get usage string
    const char* usage = (const char*)sqlite3_column_text(stmt, 8);
    if (usage) {
        help->exampleCode = std::string("Usage: ") + usage + "\n\n";
    }
    
    const char* example = (const char*)sqlite3_column_text(stmt, 9);
    if (example) {
        if (!help->exampleCode.empty()) {
            help->exampleCode += example;
        } else {
            help->exampleCode = example;
        }
    }
    
    const char* notes = (const char*)sqlite3_column_text(stmt, 10);
    if (notes) help->notes = notes;
    
    finalizeStatement(stmt);
    
    // Get parameters
    const char* paramSql = R"(
        SELECT name, type, description, is_optional, default_value, position
        FROM parameters
        WHERE command_id = ?
        ORDER BY position
    )";
    
    stmt = prepareStatement(paramSql);
    if (stmt) {
        bindInt(stmt, 1, commandId);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            CommandHelp::Parameter param;
            param.name = (const char*)sqlite3_column_text(stmt, 0);
            param.type = (const char*)sqlite3_column_text(stmt, 1);
            
            const char* desc = (const char*)sqlite3_column_text(stmt, 2);
            if (desc) param.description = desc;
            
            param.isOptional = sqlite3_column_int(stmt, 3) != 0;
            
            const char* defVal = (const char*)sqlite3_column_text(stmt, 4);
            if (defVal) param.defaultValue = defVal;
            
            param.position = sqlite3_column_int(stmt, 5);
            
            help->parameters.push_back(param);
        }
        
        finalizeStatement(stmt);
    }
    
    // Get tags
    help->tags = getCommandTags(commandName);
    
    // Get related commands
    help->relatedCommands = getRelatedCommands(commandName);
    
    return help;
}

bool HelpDatabase::hasCommand(const std::string& commandName) {
    return getCommandId(commandName) >= 0;
}

std::vector<std::string> HelpDatabase::getCommandsInCategory(const std::string& category) {
    std::vector<std::string> results;
    
    const char* sql = "SELECT name FROM commands WHERE category = ? ORDER BY name";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return results;
    }
    
    bindText(stmt, 1, category);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back((const char*)sqlite3_column_text(stmt, 0));
    }
    
    finalizeStatement(stmt);
    return results;
}

std::vector<std::string> HelpDatabase::getRelatedCommands(const std::string& commandName) {
    std::vector<std::string> results;
    
    int cmdId = getCommandId(commandName);
    if (cmdId < 0) {
        return results;
    }
    
    const char* sql = R"(
        SELECT c.name
        FROM commands c
        JOIN related_commands rc ON c.id = rc.related_command_id
        WHERE rc.command_id = ?
        ORDER BY c.name
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        return results;
    }
    
    bindInt(stmt, 1, cmdId);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back((const char*)sqlite3_column_text(stmt, 0));
    }
    
    finalizeStatement(stmt);
    return results;
}

std::vector<std::string> HelpDatabase::getCommandCategories() {
    std::vector<std::string> results;
    
    const char* sql = "SELECT DISTINCT category FROM commands ORDER BY category";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return results;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back((const char*)sqlite3_column_text(stmt, 0));
    }
    
    finalizeStatement(stmt);
    return results;
}

// =============================================================================
// Tags
// =============================================================================

std::vector<std::string> HelpDatabase::getAllTags() {
    std::vector<std::string> results;
    
    const char* sql = "SELECT name FROM tags ORDER BY name";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return results;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back((const char*)sqlite3_column_text(stmt, 0));
    }
    
    finalizeStatement(stmt);
    return results;
}

std::vector<std::string> HelpDatabase::getCommandTags(const std::string& commandName) {
    std::vector<std::string> results;
    
    int cmdId = getCommandId(commandName);
    if (cmdId < 0) {
        return results;
    }
    
    const char* sql = R"(
        SELECT t.name
        FROM tags t
        JOIN command_tags ct ON t.id = ct.tag_id
        WHERE ct.command_id = ?
        ORDER BY t.name
    )";
    
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (!stmt) {
        return results;
    }
    
    bindInt(stmt, 1, cmdId);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back((const char*)sqlite3_column_text(stmt, 0));
    }
    
    finalizeStatement(stmt);
    return results;
}

// =============================================================================
// Statistics
// =============================================================================

HelpStatistics HelpDatabase::getStatistics() {
    HelpStatistics stats;
    
    stats.commandCount = getCommandCount();
    stats.articleCount = getArticleCount();
    stats.lastRebuild = getLastRebuildTime();
    stats.schemaVersion = getSchemaVersion();
    
    const char* sql = "SELECT COUNT(*) FROM tags";
    sqlite3_stmt* stmt = prepareStatement(sql);
    if (stmt) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.tagCount = sqlite3_column_int(stmt, 0);
        }
        finalizeStatement(stmt);
    }
    
    return stats;
}

int HelpDatabase::getCommandCount() {
    const char* sql = "SELECT COUNT(*) FROM commands";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    finalizeStatement(stmt);
    return count;
}

int HelpDatabase::getArticleCount() {
    const char* sql = "SELECT COUNT(*) FROM articles";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    finalizeStatement(stmt);
    return count;
}

std::string HelpDatabase::getLastRebuildTime() {
    const char* sql = "SELECT value FROM metadata WHERE key = 'last_rebuild'";
    sqlite3_stmt* stmt = prepareStatement(sql);
    
    if (!stmt) {
        return "";
    }
    
    std::string time;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) {
            time = text;
        }
    }
    
    finalizeStatement(stmt);
    return time;
}

std::string HelpDatabase::getSchemaVersion() {
    return getDatabaseSchemaVersion();
}

// =============================================================================
// Error Handling
// =============================================================================

std::string HelpDatabase::getLastError() const {
    return m_lastError;
}

bool HelpDatabase::hasError() const {
    return !m_lastError.empty();
}

void HelpDatabase::setError(const std::string& message) {
    m_lastError = message;
    if (m_db) {
        m_lastError += ": " + std::string(sqlite3_errmsg(m_db));
    }
}

void HelpDatabase::clearError() {
    m_lastError.clear();
}

void HelpDatabase::reportProgress(const std::string& message, int current, int total) {
    if (m_progressCallback) {
        m_progressCallback(message, current, total);
    }
}

// =============================================================================
// SQL Helpers
// =============================================================================

bool HelpDatabase::execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        setError("SQL execution failed");
        if (errMsg) {
            m_lastError += ": " + std::string(errMsg);
            sqlite3_free(errMsg);
        }
        return false;
    }
    
    return true;
}

bool HelpDatabase::beginTransaction() {
    return execute("BEGIN TRANSACTION");
}

bool HelpDatabase::commitTransaction() {
    return execute("COMMIT");
}

bool HelpDatabase::rollbackTransaction() {
    return execute("ROLLBACK");
}

sqlite3_stmt* HelpDatabase::prepareStatement(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        setError("Failed to prepare statement");
        return nullptr;
    }
    
    return stmt;
}

void HelpDatabase::finalizeStatement(sqlite3_stmt* stmt) {
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

bool HelpDatabase::bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    int rc = sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    return (rc == SQLITE_OK);
}

bool HelpDatabase::bindInt(sqlite3_stmt* stmt, int index, int value) {
    int rc = sqlite3_bind_int(stmt, index, value);
    return (rc == SQLITE_OK);
}

// =============================================================================
// Global Instance
// =============================================================================

static HelpDatabase* g_globalHelpDatabase = nullptr;

HelpDatabase& getGlobalHelpDatabase() {
    if (!g_globalHelpDatabase) {
        g_globalHelpDatabase = new HelpDatabase();
    }
    return *g_globalHelpDatabase;
}

bool initializeGlobalHelpDatabase(const std::string& dbPath) {
    auto& db = getGlobalHelpDatabase();
    
    std::string path = dbPath.empty() ? HelpDatabase::getDefaultDatabasePath() : dbPath;
    
    return db.open(path);
}

// =============================================================================
// Article Documentation (Stub Implementations)
// =============================================================================

std::unique_ptr<ArticleHelp> HelpDatabase::getArticleHelp(const std::string& articleName) {
    if (!m_isOpen) {
        setError("Database not open");
        return nullptr;
    }
    
    // TODO: Implement article retrieval from database
    // For now, return nullptr (no articles in database yet)
    return nullptr;
}

bool HelpDatabase::hasArticle(const std::string& articleName) {
    if (!m_isOpen) return false;
    
    // TODO: Implement article existence check
    return false;
}

std::vector<std::string> HelpDatabase::getArticlesInCategory(const std::string& category) {
    std::vector<std::string> results;
    if (!m_isOpen) return results;
    
    // TODO: Implement category filtering for articles
    return results;
}

std::vector<std::string> HelpDatabase::getArticleCommands(const std::string& articleName) {
    std::vector<std::string> results;
    if (!m_isOpen) return results;
    
    // TODO: Implement article-command relationship query
    return results;
}

std::vector<std::string> HelpDatabase::getArticleCategories() {
    std::vector<std::string> results;
    if (!m_isOpen) return results;
    
    // TODO: Implement article category listing
    return results;
}

std::vector<std::string> HelpDatabase::getAllArticleNames() {
    std::vector<std::string> results;
    if (!m_isOpen) return results;
    
    // TODO: Implement article name listing
    // Query: SELECT name FROM articles ORDER BY name
    return results;
}



std::vector<std::string> HelpDatabase::getArticleTags(const std::string& articleName) {
    std::vector<std::string> results;
    if (!m_isOpen) return results;
    
    // TODO: Implement article tag retrieval
    return results;
}



} // namespace HelpSystem
} // namespace FasterBASIC