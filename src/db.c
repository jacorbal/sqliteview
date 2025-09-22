/**
 * @file db.c
 *
 * @brief Implementation of SQLite helper functions
 */

#define _POSIX_C_SOURCE 200809L

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Local includes */
#include <db.h>


/**
 * @brief Build a safe @c SELECT statement for rowid and all columns
 *        with a limit
 *
 * @param table Table name to include in the query (quoted in the SQL)
 *
 * @return Dynamically allocated SQL string on success (caller must
 *         free) or @c NULL on allocation/formatting error
 *
 * @note If the returned string points to a static buffer copy it must
 *       be freed by the caller
 */
static char *s_make_select_rowid_all(const char *table)
{
    char buf[256];
    int needed = snprintf(buf, sizeof(buf),
            "SELECT rowid, * FROM \"%s\" LIMIT %d;", table,
            SQL_QUERY_MAX_LIMIT);

    if (needed < 0) {
        return NULL;
    }
    if ((size_t) needed >= sizeof(buf)) {
        size_t len = (size_t) needed + 1;
        char *p = malloc(len);
        if (!p) {
            return NULL;
        }
        snprintf(p, len,
                "SELECT rowid, * FROM \"%s\" LIMIT %d;", table,
                SQL_QUERY_MAX_LIMIT);
        return p;
    } else {
        return strdup(buf);
    }
}


/* Check whether a file appears to be a valid SQLite database */
int db_is_sqlite(const char *filename)
{
    sqlite3 *tmp = NULL;
    int rc = sqlite3_open_v2(filename, &tmp,
            SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL);

    if (rc != SQLITE_OK) {
        if (tmp) sqlite3_close(tmp);
        return 0;
    }

    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(tmp, "PRAGMA schema_version;", -1,
            &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        sqlite3_close(tmp);
        return 0;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(tmp);

    return 1;
}


/* Open an SQLite database and store the handle in the context */
int db_open(context_td *s, const char *filename)
{
    if (!s) {
        return SQLITE_MISUSE;
    }
    if (s->db) {
        sqlite3_close(s->db);
        s->db = NULL;
    }

    return sqlite3_open(filename, &s->db);
}


/* Close the SQLite database in the context and clear the handle*/
void db_close(context_td *s)
{
    if (!s) {
        return;
    }

    if (s->db) {
        sqlite3_close(s->db);
        s->db = NULL;
    }
}


/* Fill the context's tables_store with table names from the database */
int db_fill_table_list(context_td *s)
{
    if (!s || !s->db || !s->tables_store) {
        return SQLITE_MISUSE;
    }

    const char *sql =
        "SELECT name FROM sqlite_master WHERE type='table' AND "
        "name NOT LIKE 'sqlite_%' ORDER BY name;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s->db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        return rc;
    }

    gtk_list_store_clear(s->tables_store);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        GtkTreeIter iter;
        gtk_list_store_append(s->tables_store, &iter);
        gtk_list_store_set(s->tables_store, &iter, 0,
                (const char*) name, -1);
    }
    sqlite3_finalize(stmt);

    /* 'sqlite3_step' returns 'SQLITE_DONE' after loop */
    return SQLITE_OK;
}


/* Free memory held for the current table name and column names */
void db_free_columns(context_td *s)
{
    if (!s) {
        return;
    }
    if (!s->current_colnames) {
        free(s->current_tablename);
        s->current_tablename = NULL;
        s->current_ncols = 0;
        return;
    }

    for (int i = 0; i < s->current_ncols; ++i) {
        free(s->current_colnames[i]);
    }
    free(s->current_colnames);
    s->current_colnames = NULL;
    s->current_ncols = 0;
    free(s->current_tablename);
    s->current_tablename = NULL;
}



/* Populate the rows view for a given table by selecting rows from the DB */
int db_populate_rows(context_td *s, const char *table)
{
    if (!s || !s->db || !s->rows_view) {
        return SQLITE_MISUSE;
    }
    if (!table) {
        return SQLITE_MISUSE;
    }

    db_free_columns(s);
    s->current_tablename = strdup(table);

    GtkTreeView *tv = GTK_TREE_VIEW(s->rows_view);
    GtkListStore *store = NULL;

    /* Clear previous model and columns */
    gtk_tree_view_set_model(tv, NULL);
    GList *cols = gtk_tree_view_get_columns(tv);
    for (GList *l = cols; l; l = l->next) {
        gtk_tree_view_remove_column(tv, GTK_TREE_VIEW_COLUMN(l->data));
    }
    g_list_free(cols);

    char *sql = s_make_select_rowid_all(table);
    if (!sql) {
        return SQLITE_NOMEM;
    }

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s->db, sql, -1, &stmt, NULL);
    free(sql);
    if (rc != SQLITE_OK) {
        return rc;
    }

    int ncol = sqlite3_column_count(stmt);
    s->current_ncols = ncol;
    s->current_colnames = calloc((size_t) ncol, sizeof(char*));
    if (!s->current_colnames) {
        sqlite3_finalize(stmt);
        return SQLITE_NOMEM;
    }

    GType *types = g_new0(GType, ncol);
    for (int i = 0; i < ncol; ++i) {
        types[i] = G_TYPE_STRING;
    }
    store = gtk_list_store_newv(ncol, types);
    g_free(types);

    /* Create columns with placeholders for renderer setup in UI (UI will
     * connect signals) */
    for (int i = 0; i < ncol; ++i) {
        const char *colname = sqlite3_column_name(stmt, i);
        s->current_colnames[i] = strdup((colname) ? colname : "");
        /* Create a simple column; UI may reconfigure/renderers later */
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
                (colname)
                ? colname
                : "", renderer, "text", i, NULL);
        gtk_tree_view_append_column(tv, col);
    }

    /* Fill rows */
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        for (int i = 0; i < ncol; ++i) {
            const unsigned char *txt = sqlite3_column_text(stmt, i);
            const char *sval = (txt) ? (const char*) txt : "";
            gtk_list_store_set(store, &iter, i, sval, -1);
        }
    }

    gtk_tree_view_set_model(tv, GTK_TREE_MODEL(store));
    g_object_unref(store);
    sqlite3_finalize(stmt);

    return SQLITE_OK;
}



/* Apply an in-place textual update to a cell in the current table */
int db_apply_update_cell(context_td *s, int colidx, const char *rowid_text, 
        const char *new_text)
{
    if (!s || !s->db || !s->current_tablename || !s->current_colnames) {
        return SQLITE_MISUSE;
    }
    if (colidx == 0) {
        return SQLITE_OK;   /* Do not edit 'rowid' */
    }
    const char *colname = s->current_colnames[colidx];
    if (!colname) {
        return SQLITE_MISUSE;
    }

    char sqlbuf[512];
    int needed = snprintf(sqlbuf, sizeof(sqlbuf),
            "UPDATE \"%s\" SET \"%s\" = ? WHERE rowid = ?;",
            s->current_tablename, colname);
    char *sql = NULL;

    if (needed < 0) {
        return SQLITE_ERROR;
    }
    if ((size_t) needed >= sizeof(sqlbuf)) {
        size_t len = (size_t) needed + 1;
        sql = malloc(len);
        if (!sql) {
            return SQLITE_NOMEM;
        }
        snprintf(sql, len,
        "UPDATE \"%s\" SET \"%s\" = ? WHERE rowid = ?;",
        s->current_tablename, colname);
    } else {
        sql = sqlbuf;
    }

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s->db, sql, -1, &stmt, NULL);
    if (sql != sqlbuf) {
        free(sql);
    }
    if (rc != SQLITE_OK) {
        if (stmt) sqlite3_finalize(stmt);
        return rc;
    }
    sqlite3_bind_text(stmt, 1, new_text, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rowid_text, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return rc;
    }
    sqlite3_finalize(stmt);

    return SQLITE_OK;
}
