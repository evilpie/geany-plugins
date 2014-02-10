/*
 *  
 *  Copyright (C) 2012  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>


GeanyPlugin      *geany_plugin;
GeanyData        *geany_data;
GeanyFunctions   *geany_functions;

PLUGIN_VERSION_CHECK(205)

PLUGIN_SET_TRANSLATABLE_INFO (
  LOCALEDIR, GETTEXT_PACKAGE,
  _("Commander"),
  _("Provides a command panel for quick access to actions, files and more"),
  VERSION,
  "Colomban Wendling <ban@herbesfolles.org>"
)


/* GTK compatibility functions/macros */

#if ! GTK_CHECK_VERSION (2, 18, 0)
# define gtk_widget_get_visible(w) \
  (GTK_WIDGET_VISIBLE (w))
# define gtk_widget_set_can_focus(w, v)               \
  G_STMT_START {                                      \
    GtkWidget *widget = (w);                          \
    if (v) {                                          \
      GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);   \
    } else {                                          \
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS); \
    }                                                 \
  } G_STMT_END
#endif

#if ! GTK_CHECK_VERSION (2, 21, 8)
# define GDK_KEY_Down       GDK_Down
# define GDK_KEY_Escape     GDK_Escape
# define GDK_KEY_ISO_Enter  GDK_ISO_Enter
# define GDK_KEY_KP_Enter   GDK_KP_Enter
# define GDK_KEY_Page_Down  GDK_Page_Down
# define GDK_KEY_Page_Up    GDK_Page_Up
# define GDK_KEY_Return     GDK_Return
# define GDK_KEY_Tab        GDK_Tab
# define GDK_KEY_Up         GDK_Up
#endif


/* Plugin */

enum {
  KB_SHOW_PANEL,
  KB_COUNT
};

struct {
  GtkWidget    *panel;
  GtkWidget    *entry;
  GtkWidget    *view;
  GtkListStore *store;
  GtkTreeModel *sort;
  
  GtkTreePath  *last_path;
} plugin_data = {
  NULL, NULL, NULL,
  NULL, NULL,
  NULL
};

typedef enum {
  COL_TYPE_MENU_ITEM  = 1 << 0,
  COL_TYPE_FILE       = 1 << 1,
  COL_TYPE_ANY        = 0xffff
} ColType;

enum {
  COL_LABEL,
  COL_PATH,
  COL_TYPE,
  COL_WIDGET,
  COL_DOCUMENT,
  COL_COUNT
};

static gboolean
is_subset (const gchar *needle, const gchar *haystack)
{
  size_t n = 0, m = 0;
  while(n < strlen(needle) && m < strlen(haystack))
  {
    if(needle[n] == haystack[m] || toupper(needle[n]) == haystack[m])
      ++n;
    ++m;
  }

  return n == strlen(needle);
}

static void
print_matrix (size_t* matrix, size_t n, size_t m, const gchar *rowLabel, const gchar *colLabel)
{
  size_t i, j;
  fprintf(stderr, "   |");
  for(j = 0; j < m; ++j)
    fprintf(stderr, "%3c", colLabel[j]);
  fprintf(stderr, "\n");

  fprintf(stderr, "---+");
  for(j = 0; j < m; ++j)
    fprintf(stderr, "---");
  fprintf(stderr, "\n");

  for(i = 0; i < n; ++i)
  {
    fprintf(stderr, " %c |", rowLabel[i]);
    for(j = 0; j < m; ++j)
    {
      fprintf(stderr, "%3zu", matrix[i*m + j]);
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}

static double
calculate_rank (const gchar *lhs, const gchar *rhs)
{
  size_t const n = strlen(lhs);
  size_t const m = strlen(rhs);
  size_t matrix[n][m], first[n], last[n];
  gboolean capitals[m];
  gboolean at_bow;
  size_t i, j, k;
  gchar ch;
  size_t capitalsTouched;
  size_t substrings;
  size_t prefixSize;
  size_t bestJIndex;
  size_t bestJLength;
  size_t len;
  gboolean foundCapital;
  size_t totalCapitals;
  double score;
  double denom;
  size_t bound;


  //bzero(matrix, sizeof(matrix));

  memset(matrix, 0, sizeof(matrix));

  //std::fill_n(&first[0], n, m);
  //std::fill_n(&last[0],  n, 0);

  for (i = 0; i < n; i++) {
    first[i] = m;
    last[i] = 0;
  }

  at_bow = TRUE;
  for(i = 0; i < m; ++i)
  {
    ch = rhs[i];
    capitals[i] = at_bow && isalnum(ch) || isupper(ch);
    at_bow = !isalnum(ch) && ch != '\'' && ch != '.';
  }

  for(i = 0; i < n; ++i)
  {
    j = i == 0 ? 0 : first[i-1] + 1;
    for(; j < m; ++j)
    {
      if(tolower(lhs[i]) == tolower(rhs[j]))
      {
        matrix[i][j] = i == 0 || j == 0 ? 1 : matrix[i-1][j-1] + 1;
        first[i]     = j < first[i] ? j : first[i];
        last[i]      = (j+1) > last[i] ? (j+1) : last[i];
      }
    }
  }

  for(i = n-1; i > 0; --i)
  {
    bound = last[i]-1;
    if(bound < last[i-1])
    {
      while(first[i-1] < bound && matrix[i-1][bound-1] == 0)
        --bound;
      last[i-1] = bound;
    }
  }

  for(i = n-1; i > 0; --i)
  {
    for(j = first[i]; j < last[i]; ++j)
    {
      if(matrix[i][j] && matrix[i-1][j-1])
        matrix[i-1][j-1] = matrix[i][j];
    }
  }

  for(i = 0; i < n; ++i)
  {
    for(j = first[i]; j < last[i]; ++j)
    {
      if(matrix[i][j] > 1 && i+1 < n && j+1 < m)
        matrix[i+1][j+1] = matrix[i][j] - 1;
    }
  }

  // print_matrix(&matrix[0][0], n, m, lhs, rhs);

  // =========================
  // = Greedy walk of Matrix =
  // =========================

  capitalsTouched = 0; // 0-n
  substrings = 0;      // 1-n
  prefixSize = 0;      // 0-m

  i = 0;
  while(i < n)
  {
    bestJIndex = 0;
    bestJLength = 0;
    for(j = first[i]; j < last[i]; ++j)
    {
      if(matrix[i][j] && capitals[j])
      {
        bestJIndex = j;
        bestJLength = matrix[i][j];

        for(k = j; k < j + bestJLength; ++k)
          capitalsTouched += capitals[k] ? 1 : 0;

        break;
      }
      else if(bestJLength < matrix[i][j])
      {
        bestJIndex = j;
        bestJLength = matrix[i][j];
      }
    }

    if (i == 0)
      prefixSize = bestJIndex;

    len = 0;
    foundCapital = FALSE;
    do {

      ++i; ++len;
      first[i] = (bestJIndex + len) > first[i] ? (bestJIndex + len) : first[i];
      if(len < bestJLength && n < 4)
      {
        if(capitals[first[i]])
          continue;

        for(j = first[i]; j < last[i] && !foundCapital; ++j)
        {
          if(matrix[i][j] && capitals[j])
            foundCapital = TRUE;
        }
      }

    } while(len < bestJLength && !foundCapital);

    ++substrings;
  }

  // ================================
  // = Calculate rank based on walk =
  // ================================

  totalCapitals = 0;
  for (i = 0; i < m; i++) {
    if (capitals[i])
      totalCapitals++;
  }
  score = 0.0;
  denom = n*(n+1) + 1;
  if(n == capitalsTouched)
  {
    score = (denom - 1) / denom;
  }
  else
  {
    score = (denom - (substrings * n + (n - capitalsTouched))) / denom;
  }
  score += (m - prefixSize) / (double)m / (2*denom);
  score += capitalsTouched / (double)totalCapitals / (4*denom);
  score += n / (double)m / (8*denom);

  printf("‘%s’ ⊂ ‘%s’: %.3f\n", lhs, rhs, score);
  return score;
}

double
rank(const gchar *filter, const gchar *candidate)
{
  if (strlen(filter) == 0) {
    return 1;
  }

  if (!is_subset(filter, candidate)) {
    return 0;
  }

  if (strcmp(filter, candidate) == 0) {
    return 1;
  }

  return calculate_rank(filter, candidate);
}

static double
key_score (const gchar *key_,
           const gchar *text_)
{
  gchar  *text  = g_utf8_casefold (text_, -1);
  gchar  *key   = g_utf8_casefold (key_, -1);
  double    score;
  
  score = rank (key, text);
  
  g_free (text);
  g_free (key);
  
  return score;
}

static const gchar *
get_key (gint *type_)
{
  gint          type  = COL_TYPE_ANY;
  const gchar  *key   = gtk_entry_get_text (GTK_ENTRY (plugin_data.entry));
  
  if (g_str_has_prefix (key, "f:")) {
    key += 2;
    type = COL_TYPE_FILE;
  } else if (g_str_has_prefix (key, "c:")) {
    key += 2;
    type = COL_TYPE_MENU_ITEM;
  }
  
  if (type_) {
    *type_ = type;
  }
  
  return key;
}

static void
tree_view_set_cursor_from_iter (GtkTreeView *view,
                                GtkTreeIter *iter)
{
  GtkTreePath *path;
  
  path = gtk_tree_model_get_path (gtk_tree_view_get_model (view), iter);
  gtk_tree_view_set_cursor (view, path, NULL, FALSE);
  gtk_tree_path_free (path);
}

static void
tree_view_move_focus (GtkTreeView    *view,
                      GtkMovementStep step,
                      gint            amount)
{
  GtkTreeIter   iter;
  GtkTreePath  *path;
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  gboolean      valid = FALSE;
  
  gtk_tree_view_get_cursor (view, &path, NULL);
  if (! path) {
    valid = gtk_tree_model_get_iter_first (model, &iter);
  } else {
    switch (step) {
      case GTK_MOVEMENT_BUFFER_ENDS:
        valid = gtk_tree_model_get_iter_first (model, &iter);
        if (valid && amount > 0) {
          GtkTreeIter prev;
          
          do {
            prev = iter;
          } while (gtk_tree_model_iter_next (model, &iter));
          iter = prev;
        }
        break;
      
      case GTK_MOVEMENT_PAGES:
        /* FIXME: move by page */
      case GTK_MOVEMENT_DISPLAY_LINES:
        gtk_tree_model_get_iter (model, &iter, path);
        if (amount > 0) {
          while ((valid = gtk_tree_model_iter_next (model, &iter)) &&
                 --amount > 0)
            ;
        } else if (amount < 0) {
          while ((valid = gtk_tree_path_prev (path)) && --amount > 0)
            ;
          
          if (valid) {
            gtk_tree_model_get_iter (model, &iter, path);
          }
        }
        break;
      
      default:
        g_assert_not_reached ();
    }
    gtk_tree_path_free (path);
  }
  
  if (valid) {
    tree_view_set_cursor_from_iter (view, &iter);
  } else {
    gtk_widget_error_bell (GTK_WIDGET (view));
  }
}

static void
tree_view_activate_focused_row (GtkTreeView *view)
{
  GtkTreePath        *path;
  GtkTreeViewColumn  *column;
  
  gtk_tree_view_get_cursor (view, &path, &column);
  if (path) {
    gtk_tree_view_row_activated (view, path, column);
    gtk_tree_path_free (path);
  }
}

static void
store_populate_menu_items (GtkListStore  *store,
                           GtkMenuShell  *menu,
                           const gchar   *parent_path)
{
  GList  *children;
  GList  *node;
  
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (node = children; node; node = node->next) {
    if (GTK_IS_SEPARATOR_MENU_ITEM (node->data) ||
        ! gtk_widget_get_visible (node->data)) {
      /* skip that */
    } else if (GTK_IS_MENU_ITEM (node->data)) {
      GtkWidget    *submenu;
      gchar        *path;
      gchar        *item_label;
      gboolean      use_underline;
      GtkStockItem  item;
      
      if (GTK_IS_IMAGE_MENU_ITEM (node->data) &&
          gtk_image_menu_item_get_use_stock (node->data) &&
          gtk_stock_lookup (gtk_menu_item_get_label (node->data), &item)) {
        item_label = g_strdup (item.label);
        use_underline = TRUE;
      } else {
        item_label = g_strdup (gtk_menu_item_get_label (node->data));
        use_underline = gtk_menu_item_get_use_underline (node->data);
      }
      
      /* remove underlines */
      if (use_underline) {
        gchar  *p   = item_label;
        gsize   len = strlen (p);
        
        while ((p = strchr (p, '_')) != NULL) {
          len -= (gsize) (p - item_label);
          
          memmove (p, p + 1, len);
        }
      }
      
      if (parent_path) {
        path = g_strconcat (parent_path, " \342\206\222 " /* right arrow */,
                            item_label, NULL);
      } else {
        path = g_strdup (item_label);
      }
      
      submenu = gtk_menu_item_get_submenu (node->data);
      if (submenu) {
        /* go deeper in the menus... */
        store_populate_menu_items (store, GTK_MENU_SHELL (submenu), path);
      } else {
        gchar *tmp;
        gchar *tooltip;
        gchar *label = g_markup_printf_escaped ("<big>%s</big>", item_label);
        
        tooltip = gtk_widget_get_tooltip_markup (node->data);
        if (tooltip) {
          SETPTR (label, g_strconcat (label, "\n<small>", tooltip, "</small>", NULL));
          g_free (tooltip);
        }
        
        tmp = g_markup_escape_text (path, -1);
        SETPTR (label, g_strconcat (label, "\n<small><i>", tmp, "</i></small>", NULL));
        g_free (tmp);
        
        gtk_list_store_insert_with_values (store, NULL, -1,
                                           COL_LABEL, label,
                                           COL_PATH, path,
                                           COL_TYPE, COL_TYPE_MENU_ITEM,
                                           COL_WIDGET, node->data,
                                           -1);
        
        g_free (label);
      }
      
      g_free (item_label);
      g_free (path);
    } else {
      g_warning ("Unknown widget type in the menu: %s",
                 G_OBJECT_TYPE_NAME (node->data));
    }
  }
  g_list_free (children);
}

static GtkWidget *
find_menubar (GtkContainer *container)
{
  GList      *children;
  GList      *node;
  GtkWidget  *menubar = NULL;
  
  children = gtk_container_get_children (container);
  for (node = children; ! menubar && node; node = node->next) {
    if (GTK_IS_MENU_BAR (node->data)) {
      menubar = node->data;
    } else if (GTK_IS_CONTAINER (node->data)) {
      menubar = find_menubar (node->data);
    }
  }
  g_list_free (children);
  
  return menubar;
}
  
static void
fill_store (GtkListStore *store)
{
  GtkWidget  *menubar;
  guint       i;
  
  /* menu items */
  menubar = find_menubar (GTK_CONTAINER (geany_data->main_widgets->window));
  store_populate_menu_items (store, GTK_MENU_SHELL (menubar), NULL);
  
  /* open files */
  foreach_document (i) {
    gchar *basename = g_path_get_basename (DOC_FILENAME (documents[i]));
    gchar *label = g_markup_printf_escaped ("<big>%s</big>\n"
                                            "<small><i>%s</i></small>",
                                            basename,
                                            DOC_FILENAME (documents[i]));
    
    gtk_list_store_insert_with_values (store, NULL, -1,
                                       COL_LABEL, label,
                                       COL_PATH, DOC_FILENAME (documents[i]),
                                       COL_TYPE, COL_TYPE_FILE,
                                       COL_DOCUMENT, documents[i],
                                       -1);
    g_free (basename);
    g_free (label);
  }
}

static gint
sort_func (GtkTreeModel  *model,
           GtkTreeIter   *a,
           GtkTreeIter   *b,
           gpointer       dummy)
{
  double        scorea;
  double        scoreb;
  gchar        *patha;
  gchar        *pathb;
  gint          typea;
  gint          typeb;
  gint          type;
  const gchar  *key = get_key (&type);
  
  gtk_tree_model_get (model, a, COL_PATH, &patha, COL_TYPE, &typea, -1);
  gtk_tree_model_get (model, b, COL_PATH, &pathb, COL_TYPE, &typeb, -1);
  
  scorea = key_score (key, patha);
  scoreb = key_score (key, pathb);
  
  // if (! (typea & type)) {
  //   scorea -= 0.5;
  // }
  // if (! (typeb & type)) {
  //   scoreb -= 0.5; // untested
  // }
  
  g_free (patha);
  g_free (pathb);
  

  if (scorea == scoreb) {
    return 0;
  }

  if (scorea > scoreb) {
    return -1;
  }

  return +1;
}

static gboolean
on_panel_key_press_event (GtkWidget    *widget,
                          GdkEventKey  *event,
                          gpointer      dummy)
{
  switch (event->keyval) {
    case GDK_KEY_Escape:
      return TRUE;
    
    case GDK_KEY_Tab:
      /* avoid leaving the entry */
      return TRUE;
    
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_ISO_Enter:
      tree_view_activate_focused_row (GTK_TREE_VIEW (plugin_data.view));
      return TRUE;
    
    case GDK_KEY_Page_Up:
    case GDK_KEY_Page_Down:
      tree_view_move_focus (GTK_TREE_VIEW (plugin_data.view),
                            GTK_MOVEMENT_PAGES,
                            event->keyval == GDK_KEY_Page_Up ? -1 : 1);
      return TRUE;
    
    case GDK_KEY_Up:
    case GDK_KEY_Down: {
      tree_view_move_focus (GTK_TREE_VIEW (plugin_data.view),
                            GTK_MOVEMENT_DISPLAY_LINES,
                            event->keyval == GDK_KEY_Up ? -1 : 1);
      return TRUE;
    }
  }
  
  return FALSE;
}

static void
on_entry_text_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    dummy)
{
  GtkTreeIter   iter;
  GtkTreeView  *view  = GTK_TREE_VIEW (plugin_data.view);
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  
  /* we force re-sorting the whole model from how it was before, and the
   * back to the new filter.  this is somewhat hackish but since we don't
   * know the original sorting order, and GtkTreeSortable don't have a
   * resort() API anyway. */
  gtk_tree_model_sort_reset_default_sort_func (GTK_TREE_MODEL_SORT (model));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (model),
                                           sort_func, NULL, NULL);
  
  if (gtk_tree_model_get_iter_first (model, &iter)) {
    tree_view_set_cursor_from_iter (view, &iter);
  }
}

static void
on_entry_activate (GtkEntry  *entry,
                   gpointer   dummy)
{
  tree_view_activate_focused_row (GTK_TREE_VIEW (plugin_data.view));
}

static void
on_panel_hide (GtkWidget *widget,
               gpointer   dummy)
{
  GtkTreeView  *view = GTK_TREE_VIEW (plugin_data.view);
  
  if (plugin_data.last_path) {
    gtk_tree_path_free (plugin_data.last_path);
    plugin_data.last_path = NULL;
  }
  gtk_tree_view_get_cursor (view, &plugin_data.last_path, NULL);
  
  gtk_list_store_clear (plugin_data.store);
}

static void
on_panel_show (GtkWidget *widget,
               gpointer   dummy)
{
  GtkTreePath *path;
  GtkTreeView *view = GTK_TREE_VIEW (plugin_data.view);
  
  fill_store (plugin_data.store);
  
  gtk_widget_grab_focus (plugin_data.entry);
  
  if (plugin_data.last_path) {
    gtk_tree_view_set_cursor (view, plugin_data.last_path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (view, plugin_data.last_path, NULL,
                                  TRUE, 0.5, 0.5);
  }
  /* make sure the cursor is set (e.g. if plugin_data.last_path wasn't valid) */
  gtk_tree_view_get_cursor (view, &path, NULL);
  if (path) {
    gtk_tree_path_free (path);
  } else {
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter_first (gtk_tree_view_get_model (view), &iter)) {
      tree_view_set_cursor_from_iter (GTK_TREE_VIEW (plugin_data.view), &iter);
    }
  }
}

static void
on_view_row_activated (GtkTreeView       *view,
                       GtkTreePath       *path,
                       GtkTreeViewColumn *column,
                       gpointer           dummy)
{
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  GtkTreeIter   iter;
  
  if (gtk_tree_model_get_iter (model, &iter, path)) {
    gint type;
    
    gtk_tree_model_get (model, &iter, COL_TYPE, &type, -1);
    
    switch (type) {
      case COL_TYPE_FILE: {
        GeanyDocument  *doc;
        gint            page;
        
        gtk_tree_model_get (model, &iter, COL_DOCUMENT, &doc, -1);
        page = document_get_notebook_page (doc);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (geany_data->main_widgets->notebook),
                                       page);
        break;
      }
      
      case COL_TYPE_MENU_ITEM: {
        GtkMenuItem *item;
        
        gtk_tree_model_get (model, &iter, COL_WIDGET, &item, -1);
        gtk_menu_item_activate (item);
        g_object_unref (item);
        
        break;
      }
    }
    gtk_widget_hide (plugin_data.panel);
  }
}

static void
create_panel (void)
{
  GtkWidget          *frame;
  GtkWidget          *box;
  GtkWidget          *scroll;
  GtkTreeViewColumn  *col;
  GtkCellRenderer    *cell;
  
  plugin_data.panel = g_object_new (GTK_TYPE_WINDOW,
                                    "decorated", FALSE,
                                    "default-width", 500,
                                    "default-height", 200,
                                    "transient-for", geany_data->main_widgets->window,
                                    "window-position", GTK_WIN_POS_CENTER_ON_PARENT,
                                    "type-hint", GDK_WINDOW_TYPE_HINT_DIALOG,
                                    "skip-taskbar-hint", TRUE,
                                    "skip-pager-hint", TRUE,
                                    NULL);
  g_signal_connect (plugin_data.panel, "focus-out-event",
                    G_CALLBACK (gtk_widget_hide), NULL);
  g_signal_connect (plugin_data.panel, "show",
                    G_CALLBACK (on_panel_show), NULL);
  g_signal_connect (plugin_data.panel, "hide",
                    G_CALLBACK (on_panel_hide), NULL);
  g_signal_connect (plugin_data.panel, "key-press-event",
                    G_CALLBACK (on_panel_key_press_event), NULL);
  
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (plugin_data.panel), frame);
  
  box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), box);
  
  plugin_data.entry = gtk_entry_new ();
  g_signal_connect (plugin_data.entry, "notify::text",
                    G_CALLBACK (on_entry_text_notify), NULL);
  g_signal_connect (plugin_data.entry, "activate",
                    G_CALLBACK (on_entry_activate), NULL);
  gtk_box_pack_start (GTK_BOX (box), plugin_data.entry, FALSE, TRUE, 0);
  
  plugin_data.store = gtk_list_store_new (COL_COUNT,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_INT,
                                          GTK_TYPE_WIDGET,
                                          G_TYPE_POINTER);
  
  plugin_data.sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (plugin_data.store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (plugin_data.sort),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);
  
  scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                         "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), scroll, TRUE, TRUE, 0);
  
  plugin_data.view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (plugin_data.sort));
  gtk_widget_set_can_focus (plugin_data.view, FALSE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (plugin_data.view), FALSE);
  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  col = gtk_tree_view_column_new_with_attributes (NULL, cell,
                                                  "markup", COL_LABEL,
                                                  NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (plugin_data.view), col);
  g_signal_connect (plugin_data.view, "row-activated",
                    G_CALLBACK (on_view_row_activated), NULL);
  gtk_container_add (GTK_CONTAINER (scroll), plugin_data.view);
  
  gtk_widget_show_all (frame);
}
  
static void
on_kb_show_panel (guint key_id)
{
  gtk_widget_show (plugin_data.panel);
}

static gboolean
on_plugin_idle_init (gpointer dummy)
{
  create_panel ();
  
  return FALSE;
}

void
plugin_init (GeanyData *data)
{
  GeanyKeyGroup *group;
  
  group = plugin_set_key_group (geany_plugin, "commander", KB_COUNT, NULL);
  keybindings_set_item (group, KB_SHOW_PANEL, on_kb_show_panel,
                        0, 0, "show_panel", _("Show Command Panel"), NULL);
  
  /* delay for other plugins to have a chance to load before, so we will
   * include their items */
  plugin_idle_add (geany_plugin, on_plugin_idle_init, NULL);
}

void
plugin_cleanup (void)
{
  if (plugin_data.panel) {
    gtk_widget_destroy (plugin_data.panel);
  }
  if (plugin_data.last_path) {
    gtk_tree_path_free (plugin_data.last_path);
  }
}

void
plugin_help (void)
{
  utils_open_browser (DOCDIR "/" PLUGIN "/README");
}
