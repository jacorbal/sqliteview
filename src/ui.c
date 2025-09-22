/**
 * @file ui.c
 *
 * @brief GTK user-interface implementation for window, widgets and UI
 *        event handlers
 */

/* System includes */
#include <stdio.h>

/* Project includes */
#include <db.h>

/* Local includes */
#include <ui.h>

/**
 * @brief Show a modal error dialog with a message
 *
 * @param parent Parent window for the dialog (may be @c NULL)
 * @param msg    Null-terminated message text to display
 */
static void s_show_error_dialog(GtkWindow *parent, const char *msg)
{
    GtkWidget *d = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", msg);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}


/**
 * @brief Show a modal informational dialog with a message
 *
 * @param parent Parent window for the dialog (may be @c NULL)
 * @param msg    Null-terminated message text to display
 */
static void s_show_info_dialog(GtkWindow *parent, const char *msg)
{
    GtkWidget *d = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", msg);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}


/**
 * @brief Handler for the "edited" signal of a @e GtkCellRendererText
 * 
 * Reads the @e rowid from the model using the provided path, updates
 * the corresponding cell in the database via @a db_apply_update_cell(),
 * and on success updates the list store cell value.
 *
 * @param cell      The @e GtkCellRendererText that emitted the signal
 * @param path      String path identifying the row in the @e GtkTreeModel
 * @param new_text  New text value entered by the user
 * @param user_data Pointer to the application context (@e context_td *)
 *
 * @note Shows an error dialog if the update fails
 */
static void s_on_cell_edited(GtkCellRendererText *cell,
        const gchar *path, const gchar *new_text, gpointer user_data)
{
    context_td *s = user_data;
    if (!s) {
        return;
    }
    gpointer p = g_object_get_data(G_OBJECT(cell), "col-index");
    int colidx = GPOINTER_TO_INT(p);

    GtkTreeModel *model =
        gtk_tree_view_get_model(GTK_TREE_VIEW(s->rows_view));
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_from_string(model, &iter, path)) {
        return;
    }
    gchar *rowid_text = NULL;
    gtk_tree_model_get(model, &iter, 0, &rowid_text, -1);
    if (!rowid_text) {
        return;
    }

    int rc = db_apply_update_cell(s, colidx, rowid_text, new_text);
    if (rc != SQLITE_OK) {
        const char *errmsg = s->db
            ? sqlite3_errmsg(s->db)
            : "Unknown DB error";
        char msg[1024];
        snprintf(msg, sizeof(msg), "Failed to update cell: %s", errmsg);
        s_show_error_dialog(GTK_WINDOW(s->win), msg);
        g_free(rowid_text);
        return;
    }

    gtk_list_store_set(GTK_LIST_STORE(model), &iter, colidx,
            new_text, -1);
    g_free(rowid_text);
}


/**
 * @brief Handler for table selection changes in the tables list
 *
 * When a table is selected, populate the rows view from the database
 * via @a db_populate_rows().  For each text cell renderer in the new
 * columns set the "editable" property and connect the "edited" signal
 * to @a s_on_cell_edited.

 * @param sel      The @e GtkTreeSelection that changed
 * @param userdata Pointer to the application context (@e context_td *)
 */
static void s_on_table_selected(GtkTreeSelection *sel,
        gpointer userdata)
{
    context_td *s = userdata;
    GtkTreeIter iter;
    GtkTreeModel *model;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        gchar *tname = NULL;
        gtk_tree_model_get(model, &iter, 0, &tname, -1);
        if (tname) {
            int rc = db_populate_rows(s, tname);
            if (rc != SQLITE_OK) {
                const char *errmsg = s->db
                    ? sqlite3_errmsg(s->db)
                    : "Unknown DB error";
                char msg[1024];
                snprintf(msg, sizeof(msg),
                        "Failed to populate rows: %s", errmsg);
                s_show_error_dialog(GTK_WINDOW(s->win), msg);
            } else {
                GtkTreeView *tv = GTK_TREE_VIEW(s->rows_view);
                GList *cols = gtk_tree_view_get_columns(tv);
                int pos = 0;

                for (GList *l = cols; l; l = l->next, ++pos) {
                    GtkTreeViewColumn *col =
                        GTK_TREE_VIEW_COLUMN(l->data);
                    GList *renderers =
                        gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(col));
                    if (renderers) {
                        for (GList *r = renderers; r; r = r->next) {
                            GtkCellRenderer *renderer =
                                GTK_CELL_RENDERER(r->data);
                            if (GTK_IS_CELL_RENDERER_TEXT(renderer)) {

                                g_object_set(renderer, "editable",
                                        (pos == 0) ? FALSE : TRUE,
                                        NULL);
                                g_object_set_data(G_OBJECT(renderer),
                                        "col-index",
                                        GINT_TO_POINTER(pos));
                                g_signal_connect(renderer, "edited",
                                        G_CALLBACK(s_on_cell_edited),
                                        s);
                            }
                        } /* ! for (GList) */
                        g_list_free(renderers);
                    } /* ! if (renderers) */
                } /* ! for (GList) */
                g_list_free(cols);
            }
            g_free(tname);
        } /* ! if (tname) */
    } /* ! if (gtk_tree_selection_get_selected) */
}


/**
 * @brief Show an "Open DB" file chooser, open the selected SQLite DB
 *        and list tables
 *
 * @param w        The widget that triggered the action (unused)
 * @param userdata Pointer to the application context (@e context_td *)
 *
 * @note Displays error or info dialogs on failure
 * @note Uses @a db_is_sqlite(), @a db_open() and @a db_fill_table_list()
 */
static void s_on_open(GtkWidget *w, gpointer userdata)
{
    (void) w;
    context_td *s = userdata;
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Open SQLite DB",
            GTK_WINDOW(s->win), GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancel", GTK_RESPONSE_CANCEL,
            "_Open", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *filename =
            gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (filename) {
            if (!db_is_sqlite(filename)) {
                char msg[1024];
                snprintf(msg, sizeof(msg),
                        "Cannot open file '%s': not an SQLite database", 
                        filename);
                s_show_error_dialog(GTK_WINDOW(s->win), msg);
                g_free(filename);
                gtk_widget_destroy(dlg);
                return;
            }
            int rc = db_open(s, filename);
            if (rc != SQLITE_OK) {
                const char *errmsg = s->db
                    ? sqlite3_errmsg(s->db)
                    : sqlite3_errstr(rc);
                char msg[1024];
                snprintf(msg, sizeof(msg),
                        "Failed to open database: %s", errmsg);
                s_show_error_dialog(GTK_WINDOW(s->win), msg);
                db_close(s);
                g_free(filename);
                gtk_widget_destroy(dlg);
                return;
            }
            rc = db_fill_table_list(s);
            if (rc != SQLITE_OK) {
                const char *errmsg = s->db
                    ? sqlite3_errmsg(s->db)
                    : sqlite3_errstr(rc);
                char msg[1024];
                snprintf(msg, sizeof(msg),
                        "Opened database but failed to list tables: %s",
                        errmsg);
                s_show_info_dialog(GTK_WINDOW(s->win), msg);
            }
            g_free(filename);
        }
    }
    gtk_widget_destroy(dlg);
}


/**
 *
 * @brief Quit handler connected to the Quit button
 *
 * @param w Widget that triggered the handler (unused)
 * @param userdata User data passed to the signal handler (unused)
 */
static void s_on_quit(GtkWidget *w, gpointer userdata)
{
    (void) w;
    (void) userdata;

    gtk_main_quit();
}


/* Build the main UI and connect signals */
void ui_build(context_td *s)
{
    s->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(s->win), 900, 600);
    g_signal_connect(s->win, "destroy", G_CALLBACK(gtk_main_quit),
            NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(s->win), vbox);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *open_btn = gtk_button_new_with_label("Open DB");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(s_on_open), s);
    gtk_box_pack_start(GTK_BOX(toolbar), open_btn, FALSE, FALSE, 0);

    GtkWidget *quit_btn = gtk_button_new_with_label("Quit");
    g_signal_connect(quit_btn, "clicked", G_CALLBACK(s_on_quit), s);
    gtk_box_pack_start(GTK_BOX(toolbar), quit_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);

    /* Left: tables list */
    s->tables_store = gtk_list_store_new(1, G_TYPE_STRING);
    s->tables_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(
                s->tables_store));
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col =
        gtk_tree_view_column_new_with_attributes("Tables", r, "text",
                0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(s->tables_view), col);
    GtkWidget *left_sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(left_sc), s->tables_view);
    gtk_paned_pack1(GTK_PANED(paned), left_sc, FALSE, TRUE);

    /* Right: rows view */
    s->rows_view = gtk_tree_view_new();
    GtkWidget *right_sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(right_sc), s->rows_view);
    gtk_paned_pack2(GTK_PANED(paned), right_sc, TRUE, TRUE);

    /* Selection handler */
    GtkTreeSelection *sel =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(s->tables_view));
    gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
    g_signal_connect(sel, "changed",
            G_CALLBACK(s_on_table_selected), s);

    gtk_widget_show_all(s->win);
}


/* Shutdown UI and release any resources (currently no-op) */
void ui_shutdown(context_td *s)
{
    /* Clear UI if needed */
    (void) s;
}
