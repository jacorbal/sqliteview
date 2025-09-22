/**
 * @file context.h
 *
 * @brief Shared context type holding GTK widgets and SQLite state
 *
 * Defines the context structure used across UI and database modules to
 * keep runtime handles and metadata: the SQLite3 database handle, main
 * window and tree views, the list store for table names, and current
 * table and column information used when populating and editing rows.
 */

#ifndef CONTEXT_H
#define CONTEXT_H

/* External includes */
#include <gtk/gtk.h>
#include <sqlite3.h>


/**
 * @struct context_td
 *
 * @brief Shared application context containing GTK widget handles and
 *        database state
 */
typedef struct {
    sqlite3 *db;                /**< SQLite database handle */
    GtkWidget *win;             /**< Main application window */
    GtkWidget *tables_view;     /**< 'GtkTreeView' showing table names */
    GtkWidget *rows_view;       /**< 'GtkTreeView' showing rows of table */
    GtkListStore *tables_store; /**< 'GtkListStore' backing 'tables_view' */
    int current_ncols;          /**< Number of columns in current model */
    char **current_colnames;    /**< Array of column name strings */
    char *current_tablename;    /**< Name of current table */
} context_td;


#endif  /* ! CONTEXT_H */
