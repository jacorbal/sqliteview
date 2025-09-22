/**
 * @file db.h
 *
 * @brief Database helper API for SQLite operations used by the UI.
 *        Provides functions to detect and open SQLite databases,
 *        populate the table list and rows view, apply cell updates and
 *        manage column metadata.
 *
 * @note Functions operate on the shared application context (@e context_td)
 */

#ifndef DB_H
#define DB_H

/* Project includes */
#include <context.h>


#define SQL_QUERY_MAX_LIMIT (100)   /**< Maximum limit for SQL queries */


/* Public interface */
/**
 * @brief Check whether a file appears to be a valid SQLite database
 *
 * Attempts to open the file read-only and run a simple pragma query.
 *
 * @param filename Path to the candidate database file
 *
 * @return 1 if the file looks like a valid SQLite database, 0 otherwise
 */
int db_is_sqlite(const char *filename);

/**
 * @brief Open an SQLite database and store the handle in the context
 *
 * @param s        Pointer to the application context (must not be @c NULL)
 * @param filename Path to the SQLite database file to open
 *
 * @return @e SQLITE_OK on success, or an SQLite error code otherwise
 *
 * @note If a database is already open in the context, it will be closed
 *       first
 */
int db_open(context_td *s, const char *filename);

/**
 * @brief Close the SQLite database in the context and clear the handle
 * 
 * @param s Pointer to the application context.
 *
 * @note Safe to call with a @c NULL context pointer
 * @note After return @e s->db will be @c NULL
*/
void db_close(context_td *s);

/**
 * @brief Fill the context's tables_store with table names from the
 *        database
 *
 * @param s Pointer to the application context
 *
 * @return @e SQLITE_OK on success or an SQLite error code on failure
 *         (or @e SQLITE_MISUSE for invalid inputs)
 *
 * @note Context must have open @e s->db and a valid @e s->tables_store
 * @note Excludes internal @a sqlite_* tables and orders names
 *       alphabetically
*/
int db_fill_table_list(context_td *s);

/**
 * @brief Free memory held for the current table name and column names
 *
 * @param s Pointer to the application context
 *
 * @note Clears @e s->current_colnames and @e s->current_tablename,
 *       and resets @e s->current_ncols
 */
void db_free_columns(context_td *s);

/**
 * @brief Populate the rows view for a given table by selecting rows
 *        from the database
 *
 * Creates a @e GtkListStore with string columns matching the result set
 * (rowid included), fills it with up to 100 rows and assigns the model
 * to @e s->rows_view.
 *
 * @param s     Pointer to the application context
 * @param table Name of the table to query (must not be NULL).
 *
 * @return @e SQLITE_OK on success or an SQLite error code on failure
 *         (or @e SQLITE_MISUSE for invalid inputs)
 *
 * @note Context must have open @e s->db and a valid @e s->rows_view
 */
int db_populate_rows(context_td *s, const char *table);

/**
 * @brief Apply an in-place textual update to a cell in the current table
 *
 * Generates and executes an UPDATE statement that sets the given column
 * to new_text for the row identified by rowid_text. Column index
 * 0 (rowid) is ignored (no-op).
 * 
 * @param s          Pointer to the application context
 * @param colidx     Column index in the current model
 * @param rowid_text Text of the rowid identifying the row to update
 * @param new_text   New text value to write into the cell
 * 
 * @return @e SQLITE_OK on success or an SQLite error code on failure
 *         (or @e SQLITE_MISUSE for invalid inputs)

 * @note Column index 0 is @e rowid and will not be updated
 * @note Context must have open @e s->db and a set @e s->current_colnames
 */
int db_apply_update_cell(context_td *s, int colidx, const char *rowid_text,
        const char *new_text);


#endif  /* ! DB_H */
