/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *    (ephy-notebook.c)
 *
 *  Copyright © 2008 Free Software Foundation, Inc.
 *    (nautilus-notebook.c)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "nautilus-notebook.h"
#include "nautilus-navigation-window.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-window-slot.h"
#include "ephy-spinner.h"
#include <libnautilus-private/nautilus-dnd.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define TAB_WIDTH_N_CHARS 15

#define AFTER_ALL_TABS -1
#define NOT_IN_APP_WINDOWS -2

#define INSANE_NUMBER_OF_URLS 20

static void nautilus_notebook_init		 (NautilusNotebook *notebook);
static void nautilus_notebook_class_init	 (NautilusNotebookClass *klass);
static int  nautilus_notebook_insert_page	 (GtkNotebook *notebook,
					  GtkWidget *child,
					  GtkWidget *tab_label,
					  GtkWidget *menu_label,
					  int position);
static void nautilus_notebook_remove	 (GtkContainer *container,
					  GtkWidget *tab_widget);

static const GtkTargetEntry url_drag_types[] = 
{
	{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST },
	{ NAUTILUS_ICON_DND_URI_LIST_TYPE, 0, NAUTILUS_ICON_DND_URI_LIST },
};

enum
{
	TAB_CLOSE_REQUEST,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (NautilusNotebook, nautilus_notebook, GTK_TYPE_NOTEBOOK);

static void
nautilus_notebook_class_init (NautilusNotebookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
	GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);

	container_class->remove = nautilus_notebook_remove;

	notebook_class->insert_page = nautilus_notebook_insert_page;
	
	gtk_rc_parse_string ("style \"nautilus-tab-close-button-style\"\n"
			     "{\n"
			       "GtkWidget::focus-padding = 0\n"
			       "GtkWidget::focus-line-width = 0\n"
			       "xthickness = 0\n"
			       "ythickness = 0\n"
			     "}\n"
			     "widget \"*.nautilus-tab-close-button\" style \"nautilus-tab-close-button-style\"");

	signals[TAB_CLOSE_REQUEST] =
		g_signal_new ("tab-close-request",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusNotebookClass, tab_close_request),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      NAUTILUS_TYPE_WINDOW_SLOT);
}


/* FIXME remove when gtknotebook's func for this becomes public, bug #.... */
static NautilusNotebook *
find_notebook_at_pointer (gint abs_x, gint abs_y)
{
	GdkWindow *win_at_pointer, *toplevel_win;
	gpointer toplevel = NULL;
	gint x, y;

	/* FIXME multi-head */
	win_at_pointer = gdk_window_at_pointer (&x, &y);
	if (win_at_pointer == NULL)
	{
		/* We are outside all windows containing a notebook */
		return NULL;
	}

	toplevel_win = gdk_window_get_toplevel (win_at_pointer);

	/* get the GtkWidget which owns the toplevel GdkWindow */
	gdk_window_get_user_data (toplevel_win, &toplevel);

	/* toplevel should be an NautilusWindow */
	if (toplevel != NULL && NAUTILUS_IS_NAVIGATION_WINDOW (toplevel))
	{
		return NAUTILUS_NOTEBOOK (NAUTILUS_NAVIGATION_WINDOW (toplevel)->notebook);
	}

	return NULL;
}

static gboolean
is_in_notebook_window (NautilusNotebook *notebook,
		       gint abs_x, gint abs_y)
{
	NautilusNotebook *nb_at_pointer;

	nb_at_pointer = find_notebook_at_pointer (abs_x, abs_y);

	return nb_at_pointer == notebook;
}

static gint
find_tab_num_at_pos (NautilusNotebook *notebook, gint abs_x, gint abs_y)
{
	GtkPositionType tab_pos;
	int page_num = 0;
	GtkNotebook *nb = GTK_NOTEBOOK (notebook);
	GtkWidget *page;

	tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

	if (GTK_NOTEBOOK (notebook)->first_tab == NULL)
	{
		return AFTER_ALL_TABS;
	}

	/* For some reason unfullscreen + quick click can
	   cause a wrong click event to be reported to the tab */
	if (!is_in_notebook_window(notebook, abs_x, abs_y))
	{
		return NOT_IN_APP_WINDOWS;
	}

	while ((page = gtk_notebook_get_nth_page (nb, page_num)))
	{
		GtkWidget *tab;
		gint max_x, max_y;
		gint x_root, y_root;

		tab = gtk_notebook_get_tab_label (nb, page);
		g_return_val_if_fail (tab != NULL, -1);

		if (!GTK_WIDGET_MAPPED (GTK_WIDGET (tab)))
		{
			page_num++;
			continue;
		}

		gdk_window_get_origin (GDK_WINDOW (tab->window),
				       &x_root, &y_root);

		max_x = x_root + tab->allocation.x + tab->allocation.width;
		max_y = y_root + tab->allocation.y + tab->allocation.height;

		if (((tab_pos == GTK_POS_TOP)
		     || (tab_pos == GTK_POS_BOTTOM))
		    &&(abs_x<=max_x))
		{
			return page_num;
		}
		else if (((tab_pos == GTK_POS_LEFT)
			  || (tab_pos == GTK_POS_RIGHT))
			 && (abs_y<=max_y))
		{
			return page_num;
		}

		page_num++;
	}
	return AFTER_ALL_TABS;
}

static gboolean
button_press_cb (NautilusNotebook *notebook,
		 GdkEventButton *event,
		 gpointer data)
{
	int tab_clicked;

	tab_clicked = find_tab_num_at_pos (notebook, event->x_root, event->y_root);

	if (event->type == GDK_BUTTON_PRESS &&
	    event->button == 3 &&
		   (event->state & gtk_accelerator_get_default_mod_mask ()) == 0)
	{
		if (tab_clicked == -1)
		{
			/* consume event, so that we don't pop up the context menu when
			 * the mouse if not over a tab label
			 */
			return TRUE;
		}

		/* switch to the page the mouse is over, but don't consume the event */
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), tab_clicked);
	}

	return FALSE;
}

static void
nautilus_notebook_init (NautilusNotebook *notebook)
{
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);

	g_signal_connect (notebook, "button-press-event",
			  (GCallback)button_press_cb, NULL);

	/* Set up drag-and-drop target */
	/* TODO this would be used for opening a new tab.
	 * It will only work properly as soon as GtkNotebook 
	 * supports to find out whether a particular point
	 * is on a tab button or not.
	 */
#if 0
	gtk_drag_dest_set (GTK_WIDGET (notebook), 0,
			   url_drag_types, G_N_ELEMENTS (url_drag_types),
			   GDK_ACTION_LINK);
	gtk_drag_dest_set_track_motion (GTK_WIDGET (notebook), TRUE);
#endif
}

void
nautilus_notebook_sync_loading (NautilusNotebook *notebook,
				NautilusWindowSlot *slot)
{
	GtkWidget *tab_label, *spinner, *icon;

	g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));
	g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));

	tab_label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), slot->content_box);
	g_return_if_fail (GTK_IS_WIDGET (tab_label));

	spinner = GTK_WIDGET (g_object_get_data (G_OBJECT (tab_label), "spinner"));
	icon = GTK_WIDGET (g_object_get_data (G_OBJECT (tab_label), "icon"));
	g_return_if_fail (spinner != NULL && icon != NULL);

	if (ephy_spinner_get_spinning (EPHY_SPINNER (spinner)) == slot->allow_stop) {
		return;
	}

	if (slot->allow_stop)
	{
		gtk_widget_hide (icon);
		gtk_widget_show (spinner);
		ephy_spinner_start (EPHY_SPINNER (spinner));
	}
	else
	{
		ephy_spinner_stop (EPHY_SPINNER (spinner));
		gtk_widget_hide (spinner);
		gtk_widget_show (icon);
	}
}

void
nautilus_notebook_sync_tab_label (NautilusNotebook *notebook,
				  NautilusWindowSlot *slot)
{
	GtkWidget *hbox, *label;
	char *location_name;

	g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));
	g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_return_if_fail (GTK_IS_WIDGET (slot->content_box));

	hbox = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), slot->content_box);
	g_return_if_fail (GTK_IS_WIDGET (hbox));

	label = GTK_WIDGET (g_object_get_data (G_OBJECT (hbox), "label"));
	g_return_if_fail (GTK_IS_WIDGET (label));

	gtk_label_set_text (GTK_LABEL (label), slot->title);

	if (slot->location != NULL) {
		/* Set the tooltip on the label's parent (the tab label hbox),
		 * so it covers all of the tab label.
		 */
		location_name = g_file_get_parse_name (slot->location);
		gtk_widget_set_tooltip_text (label->parent, location_name);
		g_free (location_name);
	} else {
		gtk_widget_set_tooltip_text (label->parent, NULL);
	}
}

static void
close_button_clicked_cb (GtkWidget *widget,
			 NautilusWindowSlot *slot)
{
	GtkWidget *notebook;

	notebook = gtk_widget_get_ancestor (slot->content_box, NAUTILUS_TYPE_NOTEBOOK);
	if (notebook != NULL) {
		g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, slot);
	}
}

static void
tab_label_style_set_cb (GtkWidget *hbox,
			GtkStyle *previous_style,
			gpointer user_data)
{
	PangoFontMetrics *metrics;
	PangoContext *context;
	GtkWidget *button;
	int char_width, h, w;

	context = gtk_widget_get_pango_context (hbox);
	metrics = pango_context_get_metrics (context,
					     hbox->style->font_desc,
					     pango_context_get_language (context));

	char_width = pango_font_metrics_get_approximate_digit_width (metrics);
	pango_font_metrics_unref (metrics);

	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (hbox),
					   GTK_ICON_SIZE_MENU, &w, &h);

	gtk_widget_set_size_request
		(hbox, TAB_WIDTH_N_CHARS * PANGO_PIXELS(char_width) + 2 * w, -1);

	button = g_object_get_data (G_OBJECT (hbox), "close-button");
	gtk_widget_set_size_request (button, w + 2, h + 2);
}

static GtkWidget *
build_tab_label (NautilusNotebook *nb, NautilusWindowSlot *slot)
{
	NautilusDragSlotProxyInfo *drag_info;
	GtkWidget *hbox, *label, *close_button, *image, *spinner, *icon;

	/* set hbox spacing and label padding (see below) so that there's an
	 * equal amount of space around the label */
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (hbox);

	/* setup load feedback */
	spinner = ephy_spinner_new ();
	ephy_spinner_set_size (EPHY_SPINNER (spinner), GTK_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);

	/* setup site icon, empty by default */
	icon = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
	/* don't show the icon */

	/* setup label */
	label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	/* setup close button */
	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button),
			       GTK_RELIEF_NONE);
	/* don't allow focus on the close button */
	gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);

	gtk_widget_set_name (close_button, "nautilus-tab-close-button");

	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (close_button, _("Close tab"));
	g_signal_connect_object (close_button, "clicked",
				 G_CALLBACK (close_button_clicked_cb), slot, 0);

	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_widget_show (image);

	gtk_box_pack_start (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);
	gtk_widget_show (close_button);

	/* Set minimal size */
	g_signal_connect (hbox, "style-set",
			  G_CALLBACK (tab_label_style_set_cb), NULL);

	drag_info = g_new0 (NautilusDragSlotProxyInfo, 1);
	drag_info->target_slot = slot;
	g_object_set_data_full (G_OBJECT (hbox), "proxy-drag-info",
				drag_info, (GDestroyNotify) g_free);

	nautilus_drag_slot_proxy_init (hbox, drag_info);

	g_object_set_data (G_OBJECT (hbox), "label", label);
	g_object_set_data (G_OBJECT (hbox), "spinner", spinner);
	g_object_set_data (G_OBJECT (hbox), "icon", icon);
	g_object_set_data (G_OBJECT (hbox), "close-button", close_button);

	return hbox;
}

static int
nautilus_notebook_insert_page (GtkNotebook *gnotebook,
			       GtkWidget *tab_widget,
			       GtkWidget *tab_label,
			       GtkWidget *menu_label,
			       int position)
{
	g_assert (GTK_IS_WIDGET (tab_widget));

	position = GTK_NOTEBOOK_CLASS (nautilus_notebook_parent_class)->insert_page (gnotebook,
										     tab_widget,
										     tab_label,
										     menu_label,
										     position);

	gtk_notebook_set_show_tabs (gnotebook,
				    gtk_notebook_get_n_pages (gnotebook) > 1);
	gtk_notebook_set_tab_reorderable (gnotebook, tab_widget, TRUE);

	return position;
}

int
nautilus_notebook_add_tab (NautilusNotebook *notebook,
			   NautilusWindowSlot *slot,
			   int position,
			   gboolean jump_to)
{
	GtkNotebook *gnotebook = GTK_NOTEBOOK (notebook);
	GtkWidget *tab_label;

	g_return_val_if_fail (NAUTILUS_IS_NOTEBOOK (notebook), -1);
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot), -1);

	tab_label = build_tab_label (notebook, slot);

	position = gtk_notebook_insert_page (GTK_NOTEBOOK (notebook),
					     slot->content_box,
					     tab_label,
					     position);

	nautilus_notebook_sync_tab_label (notebook, slot);
	nautilus_notebook_sync_loading (notebook, slot);


	/* FIXME gtk bug! */
	/* FIXME: this should be fixed in gtk 2.12; check & remove this! */
	/* The signal handler may have reordered the tabs */
	position = gtk_notebook_page_num (gnotebook, slot->content_box);

	if (jump_to)
	{
		gtk_notebook_set_current_page (gnotebook, position);

	}

	return position;
}

static void
nautilus_notebook_remove (GtkContainer *container,
			  GtkWidget *tab_widget)
{
	GtkNotebook *gnotebook = GTK_NOTEBOOK (container);
	GTK_CONTAINER_CLASS (nautilus_notebook_parent_class)->remove (container, tab_widget);

	gtk_notebook_set_show_tabs (gnotebook,
				    gtk_notebook_get_n_pages (gnotebook) > 1);

}

void
nautilus_notebook_reorder_current_child_relative (NautilusNotebook *notebook,
						  int offset)
{
	GtkNotebook *gnotebook;
	GtkWidget *child;
	int page;

	g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));

	if (!nautilus_notebook_can_reorder_current_child_relative (notebook, offset)) {
		return;
	}

	gnotebook = GTK_NOTEBOOK (notebook);

	page = gtk_notebook_get_current_page (gnotebook);
	child = gtk_notebook_get_nth_page (gnotebook, page);
	gtk_notebook_reorder_child (gnotebook, child, page + offset);
}

void
nautilus_notebook_set_current_page_relative (NautilusNotebook *notebook,
					     int offset)
{
	GtkNotebook *gnotebook;
	int page;

	g_return_if_fail (NAUTILUS_IS_NOTEBOOK (notebook));

	if (!nautilus_notebook_can_set_current_page_relative (notebook, offset)) {
		return;
	}

	gnotebook = GTK_NOTEBOOK (notebook);

	page = gtk_notebook_get_current_page (gnotebook);
	gtk_notebook_set_current_page (gnotebook, page + offset);

}

static gboolean
nautilus_notebook_is_valid_relative_position (NautilusNotebook *notebook,
					      int offset)
{
	GtkNotebook *gnotebook;
	int page;
	int n_pages;

	gnotebook = GTK_NOTEBOOK (notebook);

	page = gtk_notebook_get_current_page (gnotebook);
	n_pages = gtk_notebook_get_n_pages (gnotebook) - 1;
	if (page < 0 ||
	    (offset < 0 && page < -offset) ||
	    (offset > 0 && page > n_pages - offset)) {
		return FALSE;
	}

	return TRUE;
}

gboolean
nautilus_notebook_can_reorder_current_child_relative (NautilusNotebook *notebook,
						      int offset)
{
	g_return_val_if_fail (NAUTILUS_IS_NOTEBOOK (notebook), FALSE);

	return nautilus_notebook_is_valid_relative_position (notebook, offset);
}

gboolean
nautilus_notebook_can_set_current_page_relative (NautilusNotebook *notebook,
						 int offset)
{
	g_return_val_if_fail (NAUTILUS_IS_NOTEBOOK (notebook), FALSE);

	return nautilus_notebook_is_valid_relative_position (notebook, offset);
}

