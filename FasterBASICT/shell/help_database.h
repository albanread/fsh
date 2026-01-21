//
// help_database.h
// FasterBASIC Shell - SQLite-Based Help Database
//
// Provides fast, searchable help documentation using SQLite with FTS5.
// Populated from the modular command registry and article files.
//

#ifndef FASTERBASIC_HELP_DATABASE_H
#define FASTERBASIC_HELP_DATABASE_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <sqlite3.h>

namespace FasterBASIC {

// Forward declaration
namespace ModularCommands {
    struct CommandDefinition;
}

namespace HelpSystem {

// =============================================================================
// Data Structures
// =============================================================================

/// Search result from help queries
struct SearchResult {
    std::string name;           // Command name or article title
    std::string type;           // "command", "function", "article"
    std::string category;       // Category/topic
    std::string description;    // Short description
    std::string excerpt;        // Snippet of matching text (for articles)
    float relevance;            // Search relevance score (0.0-1.0)
    
    SearchResult() : relevance(0.0f) {}
};

/// Complete command documentation
struct CommandHelp {
    std::string name;
    std::string type;           // "command" or "function"
    std::string category;
    std::string description;
    std::string luaFunction;
    std::string returnType;     // "VOID", "INT", "FLOAT", "STRING", "BOOL"
    bool requiresParens;
    std::string exampleCode;
    std::string notes;
    
    struct Parameter {
        std::string name;
        std::string type;
        std::string description;
        bool isOptional;
        std::string defaultValue;
        int position;
    };
    
    std::vector<Parameter> parameters;
    std::vector<std::string> tags;
    std::vector<std::string> relatedCommands;
    
    CommandHelp() : requiresParens(false) {}
};

/// Tutorial article documentation
struct ArticleHelp {
    std::string name;           // URL-safe name (e.g., "getting-started-sprites")
    std::string title;          // Display title
    std::string category;       // e.g., "tutorial", "concept", "advanced"
    std::string content;        // Article body (markdown or plain text)
    std::string author;
    std::string difficulty;     // "beginner", "intermediate", "advanced"
    int estimatedTime;          // Minutes to read/complete
    std::vector<std::string> tags;
    std::vector<std::string> relatedCommands;
    
    ArticleHelp() : estimatedTime(0) {}
};

/// Database statistics
struct HelpStatistics {
    int commandCount;
    int functionCount;
    int articleCount;
    int tagCount;
    std::string lastRebuild;
    std::string schemaVersion;
    
    HelpStatistics() : commandCount(0), functionCount(0), 
                      articleCount(0), tagCount(0) {}
};

// =============================================================================
// Help Database Class
// =============================================================================

class HelpDatabase {
public:
    HelpDatabase();
    ~HelpDatabase();
    
    // Delete copy/move constructors (sqlite3* is not copyable)
    HelpDatabase(const HelpDatabase&) = delete;
    HelpDatabase& operator=(const HelpDatabase&) = delete;
    HelpDatabase(HelpDatabase&&) = delete;
    HelpDatabase& operator=(HelpDatabase&&) = delete;
    
    // =============================================================================
    // Database Lifecycle
    // =============================================================================
    
    /// Open the help database at the specified path
    /// Creates the database file if it doesn't exist
    bool open(const std::string& dbPath);
    
    /// Close the database connection
    void close();
    
    /// Check if database is currently open
    bool isOpen() const;
    
    /// Get the current database path
    std::string getDatabasePath() const;
    
    /// Get the default database path for the current platform
    static std::string getDefaultDatabasePath();
    
    // =============================================================================
    // Database Rebuild
    // =============================================================================
    
    /// Rebuild the entire database from command registry and article files
    /// This will drop all tables and recreate them from scratch
    bool rebuild();
    
    /// Check if database needs rebuild (missing tables or outdated schema)
    bool needsRebuild();
    
    /// Get progress callback for rebuild operation
    using RebuildProgressCallback = void(*)(const std::string& message, int current, int total);
    void setRebuildProgressCallback(RebuildProgressCallback callback);
    
    // =============================================================================
    // Search and Query
    // =============================================================================
    
    /// Full-text search across commands and articles
    /// @param query Search keywords (supports FTS5 syntax)
    /// @param limit Maximum number of results (default: 20)
    /// @return Vector of search results, ordered by relevance
    std::vector<SearchResult> search(const std::string& query, int limit = 20);
    
    /// Search only commands/functions
    std::vector<SearchResult> searchCommands(const std::string& query, int limit = 20);
    
    /// Search only articles
    std::vector<SearchResult> searchArticles(const std::string& query, int limit = 20);
    
    /// Find all commands/articles with a specific tag
    std::vector<SearchResult> searchByTag(const std::string& tag);
    
    /// Find commands with similar names (fuzzy matching for typos)
    /// @param partialName Partial or misspelled command name
    /// @param limit Maximum number of suggestions
    std::vector<std::string> findSimilarCommands(const std::string& partialName, int limit = 5);
    
    // =============================================================================
    // Command Documentation
    // =============================================================================
    
    /// Get complete documentation for a specific command
    /// @return nullptr if command not found
    std::unique_ptr<CommandHelp> getCommandHelp(const std::string& commandName);
    
    /// Check if command exists in database
    bool hasCommand(const std::string& commandName);
    
    /// Get list of all commands in a category
    std::vector<std::string> getCommandsInCategory(const std::string& category);
    
    /// Get list of commands related to a specific command
    std::vector<std::string> getRelatedCommands(const std::string& commandName);
    
    /// Get all available command categories
    std::vector<std::string> getCommandCategories();
    
    // =============================================================================
    // Article Documentation
    // =============================================================================
    
    /// Get complete article documentation
    /// @return nullptr if article not found
    std::unique_ptr<ArticleHelp> getArticleHelp(const std::string& articleName);
    
    /// Check if article exists in database
    bool hasArticle(const std::string& articleName);
    
    /// Get list of all articles in a category
    std::vector<std::string> getArticlesInCategory(const std::string& category);
    
    /// Get list of commands referenced by an article
    std::vector<std::string> getArticleCommands(const std::string& articleName);
    
    /// Get all available article categories
    std::vector<std::string> getArticleCategories();
    
    /// Get list of all article names
    std::vector<std::string> getAllArticleNames();
    
    // =============================================================================
    // Tags
    // =============================================================================
    
    /// Get all available tags
    std::vector<std::string> getAllTags();
    
    /// Get tags associated with a command
    std::vector<std::string> getCommandTags(const std::string& commandName);
    
    /// Get tags associated with an article
    std::vector<std::string> getArticleTags(const std::string& articleName);
    
    // =============================================================================
    // Statistics
    // =============================================================================
    
    /// Get database statistics
    HelpStatistics getStatistics();
    
    /// Get total number of commands and functions
    int getCommandCount();
    
    /// Get total number of articles
    int getArticleCount();
    
    /// Get timestamp of last database rebuild
    std::string getLastRebuildTime();
    
    /// Get schema version
    std::string getSchemaVersion();
    
    // =============================================================================
    // Error Handling
    // =============================================================================
    
    /// Get last error message
    std::string getLastError() const;
    
    /// Check if last operation succeeded
    bool hasError() const;
    
private:
    // =============================================================================
    // Database Handle
    // =============================================================================
    
    sqlite3* m_db;
    bool m_isOpen;
    std::string m_dbPath;
    std::string m_lastError;
    RebuildProgressCallback m_progressCallback;
    
    // =============================================================================
    // Schema Management
    // =============================================================================
    
    /// Create all database tables and indexes
    bool createSchema();
    
    /// Drop all tables (for clean rebuild)
    bool dropAllTables();
    
    /// Check if schema exists
    bool schemaExists();
    
    /// Get current schema version from database
    std::string getDatabaseSchemaVersion();
    
    /// Update metadata table
    bool updateMetadata(const std::string& key, const std::string& value);
    
    // =============================================================================
    // Population from Command Registry
    // =============================================================================
    
    /// Populate database from the global command registry
    bool populateFromRegistry();
    
    /// Insert a single command into database
    bool insertCommand(const ModularCommands::CommandDefinition* cmd);
    
    /// Insert command parameters
    bool insertParameters(int commandId, const ModularCommands::CommandDefinition* cmd);
    
    /// Insert command tags
    bool insertCommandTags(int commandId, const std::vector<std::string>& tags);
    
    /// Insert related commands
    bool insertRelatedCommands(int commandId, const std::vector<std::string>& relatedNames);
    
    /// Get or create tag ID
    int getOrCreateTag(const std::string& tagName);
    
    /// Get command ID by name
    int getCommandId(const std::string& commandName);
    
    // =============================================================================
    // Population from Article Files
    // =============================================================================
    
    /// Populate database from article files
    bool populateFromArticles();
    
    /// Load articles from filesystem
    std::vector<ArticleHelp> loadArticlesFromFiles();
    
    /// Parse a single article file (markdown with YAML frontmatter)
    std::optional<ArticleHelp> parseArticleFile(const std::string& filePath);
    
    /// Insert a single article into database
    bool insertArticle(const ArticleHelp& article);
    
    /// Insert article tags
    bool insertArticleTags(int articleId, const std::vector<std::string>& tags);
    
    /// Insert article-command references
    bool insertArticleCommands(int articleId, const std::vector<std::string>& commandNames);
    
    /// Get article ID by name
    int getArticleId(const std::string& articleName);
    
    // =============================================================================
    // SQL Helpers
    // =============================================================================
    
    /// Execute a SQL statement that doesn't return results
    bool execute(const std::string& sql);
    
    /// Begin a transaction
    bool beginTransaction();
    
    /// Commit a transaction
    bool commitTransaction();
    
    /// Rollback a transaction
    bool rollbackTransaction();
    
    /// Prepare a SQL statement
    sqlite3_stmt* prepareStatement(const std::string& sql);
    
    /// Finalize a prepared statement
    void finalizeStatement(sqlite3_stmt* stmt);
    
    /// Bind text parameter to prepared statement
    bool bindText(sqlite3_stmt* stmt, int index, const std::string& value);
    
    /// Bind integer parameter to prepared statement
    bool bindInt(sqlite3_stmt* stmt, int index, int value);
    
    /// Set error message
    void setError(const std::string& message);
    
    /// Clear error state
    void clearError();
    
    /// Report progress during rebuild
    void reportProgress(const std::string& message, int current, int total);
};

// =============================================================================
// Global Instance
// =============================================================================

/// Get the global help database instance
HelpDatabase& getGlobalHelpDatabase();

/// Initialize and open the global help database
/// @param dbPath Optional database path (uses default if empty)
/// @return true if opened successfully
bool initializeGlobalHelpDatabase(const std::string& dbPath = "");

} // namespace HelpSystem
} // namespace FasterBASIC

#endif // FASTERBASIC_HELP_DATABASE_H