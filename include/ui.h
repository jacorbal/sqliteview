/**
 * @file ui.h
 *
 * @brief UI interface for building and shutdown the GTK interface
 *
 * @note Functions operate on the shared application context (@e context_td)
 */

#ifndef UI_H
#define UI_H

/* Project includes */
#include <context.h>


/* Public interface */
/**
 * @brief Build the main UI and connect signals
 *
 * Creates the top-level window, toolbar, panes, tables list and rows
 * view, sets up the selection handler and shows all widgets.
 *
 * @param s Pointer to the context to populate with widget handles
 */
void ui_build(context_td *s);

/**
 * @brief Shutdown UI and release any resources (currently no-op)
 *
 * @param s Pointer to the application context
 */
void ui_shutdown(context_td *s);


#endif /* ! UI_H */

