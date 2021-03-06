/**
 *   Copyright (C) 2009  Cedric Tabin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Basic plugin structure, based of Geany Plugin howto :
 *       http://www.geany.org/manual/reference/howto.html
 */

#include "PluginEntry.h"


GeanyPlugin*           geany_plugin;
GeanyData*             geany_data;
GeanyFunctions*        geany_functions;

/*========================================== PLUGIN INFORMATION ==========================================================*/

PLUGIN_VERSION_CHECK(130)
PLUGIN_SET_TRANSLATABLE_INFO(
    LOCALEDIR,
    GETTEXT_PACKAGE,
    _("XML PrettyPrinter"),
    _("Formats an XML and makes it human-readable."),
    PRETTY_PRINTER_VERSION, "Cédric Tabin - http://www.astorm.ch")
PLUGIN_KEY_GROUP(prettyprinter, 1)

/*========================================== DECLARATIONS ================================================================*/

static GtkWidget* main_menu_item = NULL; /*the main menu of the plugin*/

/* declaration of the functions */
static void xml_format(GtkMenuItem *menuitem, gpointer gdata);
static void kb_run_xml_pretty_print(G_GNUC_UNUSED guint key_id);
static void config_closed(GtkWidget* configWidget, gint response, gpointer data);

void plugin_init(GeanyData *data);
void plugin_cleanup(void);
GtkWidget* plugin_configure(GtkDialog * dialog);

/*========================================== FUNCTIONS ===================================================================*/

void plugin_init(GeanyData *data)
{
    /* initializes the libxml2 */
    LIBXML_TEST_VERSION

    /* mutilanguage support */
    main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

    /* put the menu into the Tools */
    main_menu_item = gtk_menu_item_new_with_mnemonic(_("PrettyPrinter XML"));
    ui_add_document_sensitive(main_menu_item);

    gtk_widget_show(main_menu_item);
    gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);

    /* init keybindings */
    keybindings_set_item(plugin_key_group, 0, kb_run_xml_pretty_print,
                         0, 0, "run_pretty_printer_xml", _("Run the PrettyPrinter XML"),
                         main_menu_item);

    /* add activation callback */
    g_signal_connect(main_menu_item, "activate", G_CALLBACK(xml_format), NULL);
}

void plugin_cleanup(void)
{
    /* destroys the plugin */
    gtk_widget_destroy(main_menu_item);
}

GtkWidget* plugin_configure(GtkDialog * dialog)
{
    /* creates the configuration widget */
    GtkWidget* widget = createPrettyPrinterConfigUI(dialog);
    g_signal_connect(dialog, "response", G_CALLBACK(config_closed), NULL);
    return widget;
}

/*========================================== LISTENERS ===================================================================*/

void config_closed(GtkWidget* configWidget, gint response, gpointer gdata)
{
    /* if the user clicked OK or APPLY, then save the settings */
    if (response == GTK_RESPONSE_OK ||
        response == GTK_RESPONSE_APPLY)
    {
        saveSettings();
    }
}

void kb_run_xml_pretty_print(G_GNUC_UNUSED guint key_id)
{
    xml_format(NULL, NULL);
}

void xml_format(GtkMenuItem* menuitem, gpointer gdata)
{
    /* retrieves the current document */
    GeanyDocument* doc = document_get_current();
    GeanyEditor* editor;
    ScintillaObject* sco;
    int length;
    char* buffer;
    xmlDoc* parsedDocument;
    int result;
    int xOffset;
    GeanyFiletype* fileType;
    
    g_return_if_fail(doc != NULL);

    editor = doc->editor;
    sco = editor->sci;

    /* default printing options */
    if (prettyPrintingOptions == NULL) { prettyPrintingOptions = createDefaultPrettyPrintingOptions(); }

    /* prepare the buffer that will contain the text
     * from the scintilla object */
    length = sci_get_length(sco)+1;
    buffer = (char*)malloc(length*sizeof(char));
    if (buffer == NULL) { exit(-1); } /* malloc error */

    /* retrieves the text */
    sci_get_text(sco, length, buffer);

    /* checks if the data is an XML format */
    parsedDocument = xmlParseDoc((unsigned char*)buffer);

    /* this is not a valid xml => exit with an error message */
    if(parsedDocument == NULL)
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Unable to parse the content as XML."));
        return;
    }

    /* free all */
    xmlFreeDoc(parsedDocument);

    /* process pretty-printing */
    result = processXMLPrettyPrinting(&buffer, &length, prettyPrintingOptions);
    if (result != PRETTY_PRINTING_SUCCESS)
    {
        dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Unable to process PrettyPrinting on the specified XML because some features are not supported.\n\nSee Help > Debug messages for more details..."));
        return;
    }

    /* updates the document */
    sci_set_text(sco, buffer);

    /* set the line */
    xOffset = scintilla_send_message(sco, SCI_GETXOFFSET, 0, 0);
    scintilla_send_message(sco, SCI_LINESCROLL, -xOffset, 0); /* TODO update with the right function-call for geany-0.19 */

    /* sets the type */
    fileType = filetypes_index(GEANY_FILETYPES_XML);
    document_set_filetype(doc, fileType);
}
