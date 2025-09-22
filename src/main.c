/**
 * @file main.c
 *
 * @brief Visual tool to edit SQLite databases with GTK+3 interface
 *
 * @author J. A. Corbal <jacorbal@gmail.com>
 * @date Creation date: Mon Sep 22 07:57:05 UTC 2025
 * @date Last update: Mon Sep 22 10:35:34 UTC 2025
 * @version 0.1.0
 */

/* System includes */
#include <string.h>

/* Project includes */
#include <context.h>
#include <db.h>
#include <ui.h>


/* Main entry */
int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    context_td state;

    memset(&state, 0, sizeof(state));

    ui_build(&state);       /* Build the UI, open DB, and connect handlers */
    gtk_main();             /* GTK main event loop */

    db_free_columns(&state);    /* Free memory */
    db_close(&state);           /* Close the SQLite database */

    return 0;
}
