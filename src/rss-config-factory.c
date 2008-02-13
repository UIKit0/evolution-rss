/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2008 Lucian Langa <cooly@gnome.eu.org> 
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or 
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H

#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

#include "rss.h"

#define RSS_CONTROL_ID  "OAFIID:GNOME_Evolution_RSS:" EVOLUTION_VERSION_STRING
#define FACTORY_ID      "OAFIID:GNOME_Evolution_RSS_Factory:" EVOLUTION_VERSION_STRING

static void feeds_dialog_edit(GtkDialog *d, gpointer data);

static void
set_sensitive (GtkCellLayout   *cell_layout,
               GtkCellRenderer *cell,
               GtkTreeModel    *tree_model,
               GtkTreeIter     *iter,
               gpointer         data)
{
  GtkTreePath *path;
  gint *indices;
  gboolean sensitive = 1;

  path = gtk_tree_model_get_path (tree_model, iter);
  indices = gtk_tree_path_get_indices (path);

  if (indices[0] == 1)
#ifdef HAVE_WEBKIT
        sensitive = 1;
#else
        sensitive = 0;
#endif

  if (indices[0] == 2)
#ifdef HAVE_GTKMOZEMBED
        sensitive = 1;
#else
        sensitive = 0;
#endif


  gtk_tree_path_free (path);

  g_object_set (cell, "sensitive", sensitive, NULL);
}

static struct {
        const char *label;
        const int key;
} engines[] = {
        { N_("GtkHTML"), 0 },
        { N_("WebKit"), 1 },
        { N_("Mozilla"), 2 },
};

static void
render_engine_changed (GtkComboBox *dropdown, GCallback *user_data)
{
        int id = gtk_combo_box_get_active (dropdown);
        GtkTreeModel *model;
        GtkTreeIter iter;

        model = gtk_combo_box_get_model (dropdown);
        if (id == -1 || !gtk_tree_model_iter_nth_child (model, &iter, NULL, id))
                return;
        if (!id) id = 10;
        gconf_client_set_int(rss_gconf, GCONF_KEY_HTML_RENDER, id, NULL);
#ifdef HAVE_GTKMOZEMBED
        if (id == 2)
                rss_mozilla_init();
#endif
}

static void
enable_toggle_cb(GtkCellRendererToggle *cell,
               gchar *path_str,
               gpointer data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreeIter  iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gchar *name;
  gboolean fixed;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &fixed, -1);
  gtk_tree_model_get (model, &iter, 1, &name, -1);
  fixed ^= 1;
  g_hash_table_replace(rf->hre,
        g_strdup(lookup_key(name)),
        GINT_TO_POINTER(fixed));
  gtk_list_store_set (GTK_LIST_STORE (model),
                        &iter,
                        0,
                        fixed,
                        -1);
  gtk_tree_path_free (path);
  save_gconf_feed();
  g_free(name);
}


static void
treeview_row_activated(GtkTreeView *treeview,
                       GtkTreePath *path, GtkTreeViewColumn *column)
{
        feeds_dialog_edit((GtkDialog *)treeview, treeview);
}

static void
rep_check_cb (GtkWidget *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    /* Save the new setting to gconf */
    gconf_client_set_bool (rss_gconf, GCONF_KEY_REP_CHECK, active, NULL);
    //if we already have a timeout set destroy it first
    if (rf->rc_id && !active)
         g_source_remove(rf->rc_id);
         if (active)
         {
             //we have to make sure we have a timeout value
             if (!gconf_client_get_float(rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT, NULL))
                        gconf_client_set_float (rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT,
             gtk_spin_button_get_value((GtkSpinButton *)data), NULL);
             if (rf->rc_id)
                        g_source_remove(rf->rc_id);
		rf->rc_id = g_timeout_add (60 * 1000 * gtk_spin_button_get_value((GtkSpinButton *)data),
                                          (GtkFunction) update_articles,
                                          (gpointer)1);
         }
}

static void
rep_check_timeout_cb (GtkWidget *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data));
    gconf_client_set_float (rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT,
                gtk_spin_button_get_value((GtkSpinButton*)widget), NULL);
    if (active)
    {
        if (rf->rc_id)
                g_source_remove(rf->rc_id);
        rf->rc_id = g_timeout_add (60 * 1000 * gtk_spin_button_get_value((GtkSpinButton *)widget),
                           (GtkFunction) update_articles,
                           (gpointer)1);
    }
}

static void
host_proxy_cb (GtkWidget *widget, gpointer data)
{
    gconf_client_set_string (rss_gconf, GCONF_KEY_HOST_PROXY,
                gtk_entry_get_text((GtkEntry*)widget), NULL);
}

static void
port_proxy_cb (GtkWidget *widget, gpointer data)
{
    gconf_client_set_int (rss_gconf, GCONF_KEY_PORT_PROXY,
                gtk_spin_button_get_value_as_int((GtkSpinButton*)widget), NULL);
}

static void
close_details_cb (GtkWidget *widget, gpointer data)
{
        gtk_widget_hide(data);
}

static void
set_string_cb (GtkWidget *widget, gpointer data)
{
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (widget));
    gconf_client_set_string (rss_gconf, data, text, NULL);
}

static void
details_cb (GtkWidget *widget, gpointer data)
{
        GtkWidget *details = glade_xml_get_widget(data, "http-proxy-details");
        GtkWidget *close = glade_xml_get_widget(data, "closebutton2");
        GtkWidget *proxy_auth = glade_xml_get_widget(data, "proxy_auth");
        GtkWidget *proxy_user = glade_xml_get_widget(data, "proxy_user");
        GtkWidget *proxy_pass = glade_xml_get_widget(data, "proxy_pass");
        g_signal_connect(close, "clicked", G_CALLBACK(close_details_cb), details);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (proxy_auth),
                gconf_client_get_bool(rss_gconf, GCONF_KEY_AUTH_PROXY, NULL));
        g_signal_connect(proxy_auth, "clicked", G_CALLBACK(start_check_cb), GCONF_KEY_AUTH_PROXY);

        gchar *user = gconf_client_get_string(rss_gconf, GCONF_KEY_USER_PROXY, NULL);
        if (user)
                gtk_entry_set_text(GTK_ENTRY(proxy_user), user);
        g_signal_connect(proxy_user, "changed", G_CALLBACK(set_string_cb), GCONF_KEY_USER_PROXY);
        gchar *pass = gconf_client_get_string(rss_gconf, GCONF_KEY_PASS_PROXY, NULL);
        if (pass)
                gtk_entry_set_text(GTK_ENTRY(proxy_pass), pass);
        g_signal_connect(proxy_pass, "changed", G_CALLBACK(set_string_cb), GCONF_KEY_PASS_PROXY);

        gtk_widget_show(details);
}

static void
construct_list(gpointer key, gpointer value, gpointer user_data)
{
        GtkListStore  *store = user_data;
        GtkTreeIter    iter;

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                0, g_hash_table_lookup(rf->hre, lookup_key(key)),
                1, key,
                2, g_hash_table_lookup(rf->hrt, lookup_key(key)),
                -1);
}

static void
del_days_cb (GtkWidget *widget, add_feed *data)
{
        guint adj = gtk_spin_button_get_value((GtkSpinButton*)widget);
        data->del_days = adj;
}

static void
del_messages_cb (GtkWidget *widget, add_feed *data)
{
        guint adj = gtk_spin_button_get_value((GtkSpinButton*)widget);
        data->del_messages = adj;
}

add_feed *
create_dialog_add(gchar *text, gchar *feed_text)
{
  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox1;
  GtkWidget *hbox1;
  GtkWidget *label1;
  GtkWidget *label2;
  GtkWidget *entry1;
  GtkWidget *checkbutton1;
  GtkWidget *checkbutton2;
  GtkWidget *checkbutton3, *checkbutton4;
  GtkWidget *dialog_action_area1;
  GtkWidget *cancelbutton1;
  GtkWidget *okbutton1;
  add_feed *feed = g_new0(add_feed, 1);
  gboolean fhtml = FALSE;
  gboolean enabled = TRUE;
  gboolean del_unread = FALSE;
  guint del_feed = 0;
  guint del_days = 10;
  guint del_messages = 10;
  GtkAccelGroup *accel_group = gtk_accel_group_new ();
  gchar *flabel = NULL;

  dialog1 = gtk_dialog_new ();
  gtk_window_set_keep_above(GTK_WINDOW(dialog1), TRUE);

  if (text != NULL)
        gtk_window_set_title (GTK_WINDOW (dialog1), _("Edit Feed"));
  else
        gtk_window_set_title (GTK_WINDOW (dialog1), _("Add Feed"));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog1), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (dialog1), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (dialog1), FALSE);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  gtk_widget_show (dialog_vbox1);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox1), 9);

  label2 = gtk_label_new (_("Feed URL: "));
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0);

  entry1 = gtk_entry_new ();
  gtk_widget_show (entry1);
  gtk_box_pack_start (GTK_BOX (hbox1), entry1, TRUE, TRUE, 0);
  gtk_entry_set_invisible_char (GTK_ENTRY (entry1), 8226);
  //editing
  if (text != NULL)
  {
        gtk_entry_set_text(GTK_ENTRY(entry1), text);
        fhtml = GPOINTER_TO_INT(
                g_hash_table_lookup(rf->hrh,
                                lookup_key(feed_text)));
        enabled = GPOINTER_TO_INT(
                g_hash_table_lookup(rf->hre,
                                lookup_key(feed_text)));
        del_feed = GPOINTER_TO_INT(
                g_hash_table_lookup(rf->hrdel_feed,
                                lookup_key(feed_text)));
        del_unread = GPOINTER_TO_INT(
                g_hash_table_lookup(rf->hrdel_unread,
                                lookup_key(feed_text)));
        feed->del_days = GPOINTER_TO_INT(
                g_hash_table_lookup(rf->hrdel_days,
                                lookup_key(feed_text)));
        feed->del_messages = GPOINTER_TO_INT(
                g_hash_table_lookup(rf->hrdel_messages,
                                lookup_key(feed_text)));
  }

  gboolean validate = 1;


  GtkWidget *entry2;
  if (text != NULL)
  {
        GtkWidget *hboxt = gtk_hbox_new (FALSE, 0);
        gtk_widget_show (hboxt);
        gtk_box_pack_start (GTK_BOX (vbox1), hboxt, FALSE, FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (hboxt), 9);

        flabel = g_strdup_printf("%s: <b>%s</b>", _("Folder"),
                        lookup_feed_folder(feed_text));
        GtkWidget *labelt = gtk_label_new (flabel);
        gtk_label_set_use_markup(GTK_LABEL(labelt), 1);
        gtk_widget_show (labelt);
        gtk_box_pack_start (GTK_BOX (hboxt), labelt, FALSE, FALSE, 0);
  }
  else
  {
        entry2 = gtk_label_new (NULL);
        gtk_widget_show (entry2);
        gtk_box_pack_start (GTK_BOX (vbox1), entry2, TRUE, TRUE, 0);
        gtk_entry_set_invisible_char (GTK_ENTRY (entry2), 8226);
  }

  label1 = gtk_label_new (_("<b>Articles Settings</b>"));
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, FALSE, FALSE, 0);
  gtk_label_set_use_markup (GTK_LABEL (label1), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.5);

  checkbutton1 = gtk_check_button_new_with_mnemonic (
                _("Show article's summary"));
  gtk_widget_show (checkbutton1);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton1, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton1), 1-fhtml);

  checkbutton2 = gtk_check_button_new_with_mnemonic (
                _("Feed Enabled"));
  gtk_widget_show (checkbutton2);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton2, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton2), enabled);

  checkbutton3 = gtk_check_button_new_with_mnemonic (
                _("Validate feed"));
  if (text)
        gtk_widget_set_sensitive(checkbutton3, FALSE);

  gtk_widget_show (checkbutton3);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton3, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton3), validate);


GtkWidget *hbox2, *label3;
GtkWidget *radiobutton1, *radiobutton2, *radiobutton3;
GtkWidget *spinbutton1, *spinbutton2;
GtkObject *spinbutton1_adj, *spinbutton2_adj;
GSList *radiobutton1_group = NULL;

 //editing
// if (text != NULL)
// {
  label1 = gtk_label_new (_("<b>Articles Storage</b>"));
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, FALSE, FALSE, 0);
  gtk_label_set_use_markup (GTK_LABEL (label1), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.5);
  radiobutton1 = gtk_radio_button_new_with_mnemonic (NULL, _("Don't delete articles"));
  gtk_widget_show (radiobutton1);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton1, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton1), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
  hbox1 = gtk_hbox_new (FALSE, 10);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
  radiobutton2 = gtk_radio_button_new_with_mnemonic (NULL, _("Delete all but the last"));
  gtk_widget_show (radiobutton2);
  gtk_box_pack_start (GTK_BOX (hbox1), radiobutton2, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton2), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
  spinbutton1_adj = gtk_adjustment_new (10, 1, 1000, 1, 10, 10);
  spinbutton1 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton1_adj), 1, 0);
  gtk_widget_show (spinbutton1);
  if (feed->del_messages)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton1), feed->del_messages);
  g_signal_connect(spinbutton1, "changed", G_CALLBACK(del_messages_cb), feed);
  gtk_box_pack_start (GTK_BOX (hbox1), spinbutton1, FALSE, TRUE, 0);
  label2 = gtk_label_new (_("messages"));
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0);
  hbox2 = gtk_hbox_new (FALSE, 10);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, FALSE, 0);
  radiobutton3 = gtk_radio_button_new_with_mnemonic (NULL, _("Delete articles older than"));
  gtk_widget_show (radiobutton3);
  gtk_box_pack_start (GTK_BOX (hbox2), radiobutton3, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton3), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
  switch (del_feed)
  {
        case 1:         //all but the last
                gtk_toggle_button_set_active(
                        GTK_TOGGLE_BUTTON(radiobutton2), 1);
                break;
        case 2:         //older than days
                gtk_toggle_button_set_active(
                        GTK_TOGGLE_BUTTON(radiobutton3), 1);
                break;
        default:
                gtk_toggle_button_set_active(
                        GTK_TOGGLE_BUTTON(radiobutton1), 1);
  }
  spinbutton2_adj = gtk_adjustment_new (10, 1, 365, 1, 10, 10);
  spinbutton2 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton2_adj), 1, 0);
  if (feed->del_days)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton2), feed->del_days);
  gtk_widget_show (spinbutton2);
  g_signal_connect(spinbutton2, "changed", G_CALLBACK(del_days_cb), feed);
  gtk_box_pack_start (GTK_BOX (hbox2), spinbutton2, FALSE, FALSE, 0);
  label3 = gtk_label_new (_("day(s)"));
  gtk_widget_show (label3);
  gtk_box_pack_start (GTK_BOX (hbox2), label3, FALSE, FALSE, 0);
  checkbutton4 = gtk_check_button_new_with_mnemonic (_("Always delete unread articles"));
  gtk_widget_show (checkbutton4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton4), del_unread);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton4, FALSE, FALSE, 0);

  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  cancelbutton1 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), cancelbutton1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_CAN_DEFAULT);

  okbutton1 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), okbutton1, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton1, GTK_CAN_DEFAULT);

  gtk_widget_add_accelerator (okbutton1, "activate", accel_group,
                              GDK_Return, (GdkModifierType) 0,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator (okbutton1, "activate", accel_group,
                              GDK_KP_Enter, (GdkModifierType) 0,
                              GTK_ACCEL_VISIBLE);
  gtk_window_add_accel_group (GTK_WINDOW (dialog1), accel_group);

  gint result = gtk_dialog_run(GTK_DIALOG(dialog1));
  switch (result)
  {
    case GTK_RESPONSE_OK:
        feed->feed_url = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry1)));
        fhtml = gtk_toggle_button_get_active (
                GTK_TOGGLE_BUTTON (checkbutton1));
        fhtml ^= 1;
        feed->fetch_html = fhtml;
        enabled = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(checkbutton2));
        feed->enabled = enabled;
        validate = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(checkbutton3));
        feed->validate = validate;
        guint i=0;
        while (i<3) {
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton1)))
                        break;
                i++;
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton2)))
                        break;
                i++;
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton3)))
                        break;
        }
        feed->del_feed=i;
        feed->del_unread = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(checkbutton4));
        feed->del_messages = gtk_spin_button_get_value((GtkSpinButton *)spinbutton1);
        feed->del_days = gtk_spin_button_get_value((GtkSpinButton *)spinbutton2);
        feed->add = 1;
        // there's no reason to feetch feed if url isn't changed
        if (text && !strncmp(text, feed->feed_url, strlen(text)))
                feed->changed = 0;
        else
                feed->changed = 1;
        break;
    default:
        feed->add = 0;
        gtk_widget_destroy (dialog1);
        break;
  }
        feed->dialog = dialog1;
  if (flabel)
        g_free(flabel);
  return feed;
}



////////////////////
//feeds processing//
////////////////////

static void
feeds_dialog_add(GtkDialog *d, gpointer data)
{
        gchar *text;
        add_feed *feed = create_dialog_add(NULL, NULL);
        if (feed->feed_url && strlen(feed->feed_url))
        {
                text = feed->feed_url;
                feed->feed_url = sanitize_url(feed->feed_url);
                g_free(text);
                if (g_hash_table_find(rf->hr,
                                        check_if_match,
                                        feed->feed_url))
                {
                           rss_error(NULL, NULL, _("Error adding feed."),
                                           _("Feed already exists!"));
                           goto out;
                }
                setup_feed(feed);
                GtkTreeModel *model = gtk_tree_view_get_model ((GtkTreeView *)data);
                gtk_list_store_clear(GTK_LIST_STORE(model));
                g_hash_table_foreach(rf->hrname, construct_list, model);
                save_gconf_feed();
        }
out:    if (feed->dialog)
                gtk_widget_destroy(feed->dialog);
        g_free(feed);
}

static void
destroy_delete(GtkWidget *selector, gpointer user_data)
{
        gtk_widget_destroy(user_data);
        rf->import = 0;
}

//this function resembles emfu_delete_rec in mail/em-folder-utils.c
//which is not exported ? 
static void
rss_delete_rec (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
        while (fi) {
                CamelFolder *folder;

                d(printf ("deleting folder '%s'\n", fi->full_name));
                printf ("deleting folder '%s'\n", fi->full_name);

                if (!(folder = camel_store_get_folder (store, fi->full_name, 0, ex)))
                        return;

                        GPtrArray *uids = camel_folder_get_uids (folder);
                        int i;

                        camel_folder_freeze (folder);
                        for (i = 0; i < uids->len; i++)
                                camel_folder_delete_message (folder, uids->pdata[i]);

                        camel_folder_free_uids (folder, uids);

                        camel_folder_sync (folder, TRUE, NULL);
                        camel_folder_thaw (folder);

                camel_store_delete_folder (store, fi->full_name, ex);
                if (camel_exception_is_set (ex))
                        return;

                fi = fi->next;
        }
}

static void
rss_delete_folders (CamelStore *store, const char *full_name, CamelException *ex)
{
        guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
        CamelFolderInfo *fi;

        fi = camel_store_get_folder_info (store, full_name, flags, ex);
        if (camel_exception_is_set (ex))
                return;

        rss_delete_rec (store, fi, ex);
        camel_store_free_folder_info (store, fi);
}

void
remove_feed_hash(gpointer name)
{
	//we need to make sure we won't fetch_feed iterate over those hashes
	rf->pending = TRUE;
        g_hash_table_remove(rf->hre, lookup_key(name));
        g_hash_table_remove(rf->hrt, lookup_key(name));
        g_hash_table_remove(rf->hrh, lookup_key(name));
        g_hash_table_remove(rf->hr, lookup_key(name));
        g_hash_table_remove(rf->hrname_r, lookup_key(name));
        g_hash_table_remove(rf->hrname, name);
	rf->pending = FALSE;
}

void
delete_feed_folder_alloc(gchar *old_name)
{
        FILE *f;
        gchar *feed_dir = g_strdup_printf("%s/mail/rss",
            mail_component_peek_base_directory (mail_component_peek ()));
        if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);
        gchar *feed_file = g_strdup_printf("%s/feed_folders", feed_dir);
        g_free(feed_dir);
        f = fopen(feed_file, "wb");
        if (!f)
                        return;

        gchar *orig_name = g_hash_table_lookup(rf->feed_folders, old_name);
        if (orig_name)
                g_hash_table_remove(rf->feed_folders, old_name);

        g_hash_table_foreach(rf->feed_folders,
                                (GHFunc)write_feeds_folder_line,
                                (gpointer *)f);
        fclose(f);
        g_hash_table_destroy(rf->reversed_feed_folders);
        rf->reversed_feed_folders = g_hash_table_new_full(g_str_hash,
                                                        g_str_equal,
                                                        g_free,
                                                        g_free);
        g_hash_table_foreach(rf->feed_folders,
                                (GHFunc)populate_reversed,
                                rf->reversed_feed_folders);
}


static void
delete_response(GtkWidget *selector, guint response, gpointer user_data)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name;
        CamelException ex;
        CamelFolder *mail_folder;
        if (response == GTK_RESPONSE_OK) {
                selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(user_data));
                if (gtk_tree_selection_get_selected(selection, &model, &iter))
                {
                        gtk_tree_model_get (model, &iter, 1, &name, -1);
                        if (gconf_client_get_bool(rss_gconf, GCONF_KEY_REMOVE_FOLDER, NULL))
                        {
                                //delete folder
                                CamelStore *store = mail_component_peek_local_store(NULL);
                                gchar *full_path = g_strdup_printf("%s/%s",
                                                lookup_main_folder(),
                                                lookup_feed_folder(name));
                                delete_feed_folder_alloc(lookup_feed_folder(name));
                                camel_exception_init (&ex);
                                rss_delete_folders (store, full_path, &ex);
                                if (camel_exception_is_set (&ex))
                                {
                                        e_error_run(NULL,
                                                "mail:no-delete-folder", full_path, ex.desc, NULL);
                                        camel_exception_clear (&ex);
                                }
                                g_free(full_path);
                                //also remove status file
                                gchar *url =  g_hash_table_lookup(rf->hr,
                                                        g_hash_table_lookup(rf->hrname,
                                                        name));
                                gchar *buf = gen_md5(url);
                                gchar *feed_dir = g_strdup_printf("%s/mail/rss",
                                        mail_component_peek_base_directory (mail_component_peek ()));
                                gchar *feed_name = g_strdup_printf("%s/%s", feed_dir, buf);
                                g_free(feed_dir);
                                g_free(buf);
                                unlink(feed_name);
                        }
                        remove_feed_hash(name);
                        g_free(name);
                }
                gtk_list_store_clear(GTK_LIST_STORE(model));
                g_hash_table_foreach(rf->hrname, construct_list, model);
                save_gconf_feed();
        }
        gtk_widget_destroy(selector);
        rf->import = 0;
}

feeds_dialog_disable(GtkDialog *d, gpointer data)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name;

        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(rf->treeview));
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
                gtk_tree_model_get (model, &iter, 1, &name, -1);
                gpointer key = lookup_key(name);
                g_free(name);
                g_hash_table_replace(rf->hre, g_strdup(key),
                        GINT_TO_POINTER(!g_hash_table_lookup(rf->hre, key)));
                gtk_button_set_label(data,
                        g_hash_table_lookup(rf->hre, key) ? _("Disable") : _("Enable"));
        }
        //update list instead of rebuilding
        gtk_list_store_clear(GTK_LIST_STORE(model));
        g_hash_table_foreach(rf->hrname, construct_list, model);
        save_gconf_feed();
}

GtkWidget*
remove_feed_dialog(gchar *msg)
{
  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox1;
  GtkWidget *label1;
  GtkWidget *checkbutton1;
  GtkWidget *dialog_action_area1;
  GtkWidget *cancelbutton1;
  GtkWidget *okbutton1;

  dialog1 = gtk_dialog_new ();
  gtk_window_set_keep_above(GTK_WINDOW(dialog1), TRUE);
  gtk_window_set_title (GTK_WINDOW (dialog1), _("Delete Feed?"));
  gtk_window_set_type_hint (GTK_WINDOW (dialog1), GDK_WINDOW_TYPE_HINT_DIALOG);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  gtk_widget_show (dialog_vbox1);

  vbox1 = gtk_vbox_new (FALSE, 10);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 10);

  label1 = gtk_label_new (msg);
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, TRUE, TRUE, 0);
  gtk_label_set_use_markup (GTK_LABEL (label1), TRUE);
  gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_CENTER);

  checkbutton1 = gtk_check_button_new_with_mnemonic (_("Remove folder contents"));
  gtk_widget_show (checkbutton1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton1),
                gconf_client_get_bool(rss_gconf, GCONF_KEY_REMOVE_FOLDER, NULL));
  g_signal_connect(checkbutton1,
                "clicked",
                G_CALLBACK(start_check_cb),
                GCONF_KEY_REMOVE_FOLDER);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton1, FALSE, FALSE, 0);

  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  okbutton1 = gtk_button_new_from_stock ("gtk-delete");
  gtk_widget_show (okbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), okbutton1, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton1, GTK_CAN_DEFAULT);

  cancelbutton1 = gtk_button_new_with_label (_("Do not delete"));
  gtk_widget_show (cancelbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), cancelbutton1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_HAS_FOCUS);
  return dialog1;
}

feeds_dialog_delete(GtkDialog *d, gpointer data)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name;

        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
        if (gtk_tree_selection_get_selected(selection, &model, &iter)
                && !rf->import)
        {
                rf->import = 1;
                gtk_tree_model_get (model, &iter, 1, &name, -1);
                gchar *msg = g_strdup_printf(_("Are you sure you want\n to remove <b>%s</b>?"), name);
                GtkWidget *rfd = remove_feed_dialog(msg);
                gtk_widget_show(rfd);
                g_signal_connect(rfd, "response", G_CALLBACK(delete_response), data);
                g_signal_connect(rfd, "destroy", G_CALLBACK(destroy_delete), rfd);
                g_free(msg);
                g_free(name);
        }
}

static void
feeds_dialog_edit(GtkDialog *d, gpointer data)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name, *feed_name;
        gchar *text;
        gchar *url;

        /* This will only work in single or browse selection mode! */
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
                gtk_tree_model_get (model, &iter, 1, &feed_name, -1);
                name = g_hash_table_lookup(rf->hr, lookup_key(feed_name));
                if (name)
                {
                        add_feed *feed = create_dialog_add(name, feed_name);
                        if (!feed->add)
                                goto out;
                        text = feed->feed_url;
                        feed->feed_url = sanitize_url(feed->feed_url);
                        g_free(text);
                        url = name;
                        if (feed->feed_url)
                        {
                                gtk_tree_model_get (model, &iter, 1, &name, -1);
                                gpointer key = lookup_key(name);
                                if (strcmp(url, feed->feed_url))
                                {
                                        //prevent adding of an existing feed (url)
                                        //which might screw things
                                        if (g_hash_table_find(rf->hr,
                                                check_if_match,
                                                feed->feed_url))
                                        {
                                                rss_error(NULL, NULL, _("Error adding feed."),
                                                        _("Feed already exists!"));
                                                goto out;
                                        }
                                        gchar *value1 = g_strdup(g_hash_table_lookup(rf->hr, key));
                                      	remove_feed_hash(name);
                                        g_hash_table_remove(rf->hr, key);
                                        gpointer md5 = gen_md5(feed->feed_url);
                                        if (!setup_feed(feed))
                                        {
                                                //editing might loose a corectly setup feed
                                                //so re-add previous deleted feed
                                                g_hash_table_insert(rf->hr, g_strdup(key), value1);
                                        }
                                        else
                                                g_free(value1);
                                        gtk_list_store_clear(GTK_LIST_STORE(model));
                                        g_hash_table_foreach(rf->hrname, construct_list, model);
                                        save_gconf_feed();
                                        g_free(md5);
                                }
                                else
                                {
                                        key = gen_md5(url);
                                        g_hash_table_replace(rf->hrh,
                                                        g_strdup(key),
                                                        GINT_TO_POINTER(feed->fetch_html));
                                        g_hash_table_replace(rf->hre,
                                                        g_strdup(key),
                                                        GINT_TO_POINTER(feed->enabled));
                                        g_hash_table_replace(rf->hrdel_feed,
                                                        g_strdup(key),
                                                        GINT_TO_POINTER(feed->del_feed));
                                        g_hash_table_replace(rf->hrdel_days,
                                                        g_strdup(key),
                                                        GINT_TO_POINTER(feed->del_days));
                                        g_hash_table_replace(rf->hrdel_messages,
                                                        g_strdup(key),
                                                        GINT_TO_POINTER(feed->del_messages));
                                        g_hash_table_replace(rf->hrdel_unread,
                                                        g_strdup(key),
                                                        GINT_TO_POINTER(feed->del_unread));
                                        g_free(key);
                                        gtk_list_store_clear(GTK_LIST_STORE(model));
                                        g_hash_table_foreach(rf->hrname, construct_list, model);
                                        save_gconf_feed();
                                }
                        }
out:                    if (feed->dialog)
                                gtk_widget_destroy(feed->dialog);
                        g_free(feed);
                }
        }
}

static void
import_dialog_response(GtkWidget *selector, guint response, gpointer user_data)
{
        while (gtk_events_pending ())
                gtk_main_iteration ();
        if (response == GTK_RESPONSE_CANCEL)
                rf->cancel = 1;
}

void
import_opml(gchar *file, add_feed *feed)
{
        xmlChar *buff = NULL;
        //some defaults
        feed->changed=0;
        feed->add=1;
        guint total = 0;
        guint current = 0;
        gchar *what = NULL;
        GtkWidget *import_dialog;
        GtkWidget *import_label;
        GtkWidget *import_progress;

        xmlNode *src = (xmlNode *)xmlParseFile (file);
        xmlNode *doc = src;
        gchar *msg = g_strdup(_("Importing feeds..."));
        import_dialog = e_error_new((GtkWindow *)rf->preferences, "shell:importing", msg, NULL);
        gtk_window_set_keep_above(GTK_WINDOW(import_dialog), TRUE);
        g_signal_connect(import_dialog, "response", G_CALLBACK(import_dialog_response), NULL);
        import_label = gtk_label_new(_("Please wait"));
        import_progress = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox),
                import_label,
                FALSE,
                FALSE,
                0);
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox),
                import_progress,
                FALSE,
                FALSE,
                0);
        gtk_widget_show_all(import_dialog);
        g_free(msg);
        while (src = html_find(src, "outline"))
        {
                feed->feed_url = xmlGetProp((xmlNode *)src, "xmlUrl");
                if (feed->feed_url)
                {
                        total++;
                        xmlFree(feed->feed_url);
                }
        }
        src = doc;
        //we'll be safer this way
        rf->import = 1;
        while (gtk_events_pending ())
                gtk_main_iteration ();
        while (src = html_find(src, "outline"))
        {
                feed->feed_url = xmlGetProp((xmlNode *)src, "xmlUrl");
                if (feed->feed_url && strlen(feed->feed_url))
                {
                        if (rf->cancel)
                        {
                                if (src) xmlFree(src);
                                rf->cancel = 0;
                                goto out;
                        }
                        gchar *name = xmlGetProp((xmlNode *)src, "title");
                        gchar *safe_name = decode_html_entities(name);
                        xmlFree(name);
                        name = safe_name;

                        gtk_label_set_text(GTK_LABEL(import_label), name);
#if GTK_2_6
                        gtk_label_set_ellipsize (GTK_LABEL (import_label), PANGO_ELLIPSIZE_START);
#endif
                        feed->feed_name = name;
                        /* we'll get rid of this as soon as we fetch unblocking */
                        if (g_hash_table_find(rf->hr,
                                        check_if_match,
                                        feed->feed_url))
                        {
                           rss_error(NULL, feed->feed_name, _("Error adding feed."),
                                           _("Feed already exists!"));
                           continue;
                        }
                        guint res = setup_feed(feed);

                        while (gtk_events_pending ())
                                gtk_main_iteration ();
#if RSS_DEBUG
                        g_print("feed imported:%d\n", res);
#endif
                        current++;
                        float fr = ((current*100)/total);
                        gtk_progress_bar_set_fraction((GtkProgressBar *)import_progress, fr/100);
                        what = g_strdup_printf(_("%2.0f%% done"), fr);
                        gtk_progress_bar_set_text((GtkProgressBar *)import_progress, what);
                        g_free(what);
                        while (gtk_events_pending ())
                                gtk_main_iteration ();
                        GtkTreeModel *model = gtk_tree_view_get_model((GtkTreeView *)rf->treeview);
                        gtk_list_store_clear(GTK_LIST_STORE(model));
                        g_hash_table_foreach(rf->hrname, construct_list, model);
                        save_gconf_feed();
                        g_free(feed->feed_url);
                        if (src)
                                xmlFree(src);
                }
                else
                        src = html_find(src, "outline");

        }
        while (gtk_events_pending ())
                gtk_main_iteration ();
out:    rf->import = 0;
        xmlFree(doc);
        gtk_widget_destroy(import_dialog);
//how the hell should I free this ?
////      g_free(feed);
}

static void
select_file_response(GtkWidget *selector, guint response, gpointer user_data)
{
        if (response == GTK_RESPONSE_OK) {
                char *name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
                if (name)
                {
                        gtk_widget_hide(selector);
                        import_opml(name, user_data);
                        g_free(name);
                }
        }
        else
                gtk_widget_destroy(selector);
}

static void
import_toggle_cb_html (GtkWidget *widget, add_feed *data)
{
        data->fetch_html  = 1-gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
import_toggle_cb_valid (GtkWidget *widget, add_feed *data)
{
        data->validate  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
import_toggle_cb_ena (GtkWidget *widget, add_feed *data)
{
        data->enabled  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
decorate_import_fs (gpointer data)
{
        add_feed *feed = g_new0(add_feed, 1);
        gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (data), TRUE);
        gtk_dialog_set_default_response (GTK_DIALOG (data), GTK_RESPONSE_OK);
        gtk_file_chooser_set_local_only (data, FALSE);

        GtkFileFilter *file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("All Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.opml");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("OPML Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.xml");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("XML Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));


        GtkFileFilter *filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (filter, "*.opml");
        gtk_file_filter_add_pattern (filter, "*.xml");
        gtk_file_chooser_set_filter(data, filter);

        GtkWidget *vbox1;
        GtkWidget *checkbutton1;
        GtkWidget *checkbutton2;
        GtkWidget *checkbutton3;

        vbox1 = gtk_vbox_new (FALSE, 0);
        checkbutton1 = gtk_check_button_new_with_mnemonic (
                               _("Show article's summary"));
        gtk_widget_show (checkbutton1);
        gtk_box_pack_start (GTK_BOX (vbox1), checkbutton1, FALSE, TRUE, 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton1), 1);

        checkbutton2 = gtk_check_button_new_with_mnemonic (
                                        _("Feed Enabled"));
        gtk_widget_show (checkbutton2);
        gtk_box_pack_start (GTK_BOX (vbox1), checkbutton2, FALSE, TRUE, 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton2), 1);

        checkbutton3 = gtk_check_button_new_with_mnemonic (
                                              _("Validate feed"));

        gtk_widget_show (checkbutton3);
        gtk_box_pack_start (GTK_BOX (vbox1), checkbutton3, FALSE, TRUE, 0);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton3), 1);

        gtk_file_chooser_set_extra_widget(data, vbox1);
        feed->fetch_html = 0;
        feed->validate = feed->enabled = 1;

        g_signal_connect(checkbutton1,
                        "toggled",
                        G_CALLBACK(import_toggle_cb_html),
                        feed);
        g_signal_connect(checkbutton2,
                        "toggled",
                        G_CALLBACK(import_toggle_cb_ena),
                        feed);
        g_signal_connect(checkbutton3,
                        "toggled",
                        G_CALLBACK(import_toggle_cb_valid),
                        feed);
        g_signal_connect(data, "response", G_CALLBACK(select_file_response), feed);
        g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
}

GtkWidget*
create_import_dialog (void)
{
  GtkWidget *import_file_select;
  GtkWidget *dialog_vbox5;
  GtkWidget *dialog_action_area5;
  GtkWidget *button1;
  GtkWidget *button2;

  import_file_select = gtk_file_chooser_dialog_new (_("Select import file"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN, NULL);
  gtk_window_set_keep_above(GTK_WINDOW(import_file_select), TRUE);
  gtk_window_set_modal (GTK_WINDOW (import_file_select), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (import_file_select), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (import_file_select), GDK_WINDOW_TYPE_HINT_DIALOG);

  dialog_vbox5 = GTK_DIALOG (import_file_select)->vbox;
  gtk_widget_show (dialog_vbox5);

  dialog_action_area5 = GTK_DIALOG (import_file_select)->action_area;
  gtk_widget_show (dialog_action_area5);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area5), GTK_BUTTONBOX_END);

  button1 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (button1);
  gtk_dialog_add_action_widget (GTK_DIALOG (import_file_select), button1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (button1, GTK_CAN_DEFAULT);

  button2 = gtk_button_new_from_stock ("gtk-open");
  gtk_widget_show (button2);
  gtk_dialog_add_action_widget (GTK_DIALOG (import_file_select), button2, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (button2, GTK_CAN_DEFAULT);

  gtk_widget_grab_default (button2);
  return import_file_select;
}

GtkWidget*
create_export_dialog (void)
{
  GtkWidget *export_file_select;
  GtkWidget *vbox26;
  GtkWidget *hbuttonbox1;
  GtkWidget *button3;
  GtkWidget *button4;

  export_file_select = gtk_file_chooser_dialog_new (_("Select file to export"), NULL, GTK_FILE_CHOOSER_ACTION_SAVE, NULL);
  gtk_window_set_keep_above(GTK_WINDOW(export_file_select), TRUE);
  g_object_set (export_file_select,
                "local-only", FALSE,
                NULL);
  gtk_window_set_modal (GTK_WINDOW (export_file_select), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (export_file_select), FALSE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (export_file_select), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (export_file_select), GDK_WINDOW_TYPE_HINT_DIALOG);

  vbox26 = GTK_DIALOG (export_file_select)->vbox;
  gtk_widget_show (vbox26);

  hbuttonbox1 = GTK_DIALOG (export_file_select)->action_area;
  gtk_widget_show (hbuttonbox1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);

  button3 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (button3);
  gtk_dialog_add_action_widget (GTK_DIALOG (export_file_select), button3, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (button3, GTK_CAN_DEFAULT);

  button4 = gtk_button_new_from_stock ("gtk-save");
  gtk_widget_show (button4);
  gtk_dialog_add_action_widget (GTK_DIALOG (export_file_select), button4, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (button4, GTK_CAN_DEFAULT);

  gtk_widget_grab_default (button4);
  return export_file_select;
}

static void
import_cb (GtkWidget *widget, gpointer data)
{
        if (!rf->import)
        {
                GtkWidget *import = create_import_dialog();
                decorate_import_fs(import);
                gtk_widget_show(import);
        }
        return;
}

static void
construct_opml_line(gpointer key, gpointer value, gpointer user_data)
{
        gchar *url = g_hash_table_lookup(rf->hr, value);
        gchar *type = g_hash_table_lookup(rf->hrt, value);
        gchar *url_esc = g_markup_escape_text(url, strlen(url));
        gchar *key_esc = g_markup_escape_text(key, strlen(key));
        gchar *tmp = g_strdup_printf("<outline text=\"%s\" title=\"%s\" type=\"%s\" xmlUrl=\"%s\" htmlUrl=\"%s\"/>\n",
                key_esc, key_esc, type, url_esc, url_esc);
        if (buffer != NULL)
                buffer = g_strconcat(buffer, tmp, NULL);
        else
                buffer = g_strdup(tmp);
        g_free(tmp);
        count++;
        float fr = ((count*100)/g_hash_table_size(rf->hr));
        gtk_progress_bar_set_fraction((GtkProgressBar *)user_data, fr/100);
        gchar *what = g_strdup_printf(_("%2.0f%% done"), fr);
        gtk_progress_bar_set_text((GtkProgressBar *)user_data, what);
        g_free(what);
}

void
export_opml(gchar *file)
{
        GtkWidget *import_dialog;
        GtkWidget *import_label;
        GtkWidget *import_progress;
        char outstr[200];
        time_t t;
        struct tm *tmp;
        int btn = GTK_RESPONSE_YES;
        FILE *f;


        gchar *msg = g_strdup(_("Exporting feeds..."));
        import_dialog = e_error_new((GtkWindow *)rf->preferences, "shell:importing", msg, NULL);
        gtk_window_set_keep_above(GTK_WINDOW(import_dialog), TRUE);
//        g_signal_connect(import_dialog, "response", G_CALLBACK(import_dialog_response), NULL);
        import_label = gtk_label_new(_("Please wait"));
        import_progress = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox), import_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox), import_progress, FALSE, FALSE, 0);
        gtk_widget_show_all(import_dialog);
        g_free(msg);
        count = 0;
        g_hash_table_foreach(rf->hrname, construct_opml_line, import_progress);
        gtk_widget_destroy(import_dialog);
        t = time(NULL);
        tmp = localtime(&t);
        strftime(outstr, sizeof(outstr), "%a, %d %b %Y %H:%M:%S %z", tmp);
        gchar *opml = g_strdup_printf("<opml version=\"1.1\">\n<head>\n"
                "<title>Evolution-RSS Exported Feeds</title>\n"
                "<dateModified>%s</dateModified>\n</head>\n<body>%s</body>\n</opml>\n",
                outstr,
                buffer);
        g_free(buffer);

        if (g_file_test (file, G_FILE_TEST_IS_REGULAR)) {
                GtkWidget *dlg;

                dlg = gtk_message_dialog_new (GTK_WINDOW (rf->preferences), 0,
                                              GTK_MESSAGE_QUESTION,
                                              GTK_BUTTONS_YES_NO,
                                              _("A file by that name already exists.\n"
                                                "Overwrite it?"));
                gtk_window_set_title (GTK_WINDOW (dlg), _("Overwrite file?"));
                gtk_dialog_set_has_separator (GTK_DIALOG (dlg), FALSE);

                btn = gtk_dialog_run (GTK_DIALOG (dlg));
                gtk_widget_destroy (dlg);
        }

        if (btn == GTK_RESPONSE_YES)
                goto over;
        else
                goto out;

over:   f = fopen(file, "w+");
        if (f)
        {
                fwrite(opml, strlen(opml), 1, f);
                fclose(f);
        }
        else
        {
                e_error_run(NULL,
                        "org-gnome-evolution-rss:feederr",
                        _("Error exporting feeds!"),
                        g_strerror(errno),
                        NULL);
        }
out:    g_free(opml);

}

static void
select_export_response(GtkWidget *selector, guint response, gpointer user_data)
{
        if (response == GTK_RESPONSE_OK) {
                char *name;

                name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
                if (name)
                {
                        gtk_widget_destroy(selector);
                        export_opml(name);
                        g_free(name);
                }
        }
        else
                gtk_widget_destroy(selector);

}

static void
decorate_export_fs (gpointer data)
{
        add_feed *feed = g_new0(add_feed, 1);
        gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (data), TRUE);
        gtk_dialog_set_default_response (GTK_DIALOG (data), GTK_RESPONSE_OK);
        gtk_file_chooser_set_local_only (data, FALSE);

        GtkFileFilter *file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("All Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.opml");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("OPML Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.xml");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("XML Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));


        GtkFileFilter *filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (filter, "*.opml");
        gtk_file_filter_add_pattern (filter, "*.xml");
        gtk_file_chooser_set_filter(data, filter);
        g_signal_connect(data, "response", G_CALLBACK(select_export_response), data);
        g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
}

static void
export_cb (GtkWidget *widget, gpointer data)
{
        if (!rf->import)
        {
                GtkWidget *export = create_export_dialog();
                decorate_export_fs(export);
                gtk_dialog_set_default_response (GTK_DIALOG (export), GTK_RESPONSE_OK);
                if (g_hash_table_size(rf->hrname)<1)
                {
                        e_error_run(NULL,
                                "org-gnome-evolution-rss:generr",
                                _("No RSS feeds configured!\nUnable to export."),
                                NULL);
                        return;
                }
                gtk_widget_show(export);

//              g_signal_connect(data, "response", G_CALLBACK(select_export_response), data);
//              g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
        }
        return;
}

/*=============*
 * BONOBO part *
 *=============*/

EvolutionConfigControl*
rss_config_control_new (void)
{
        GtkWidget *control_widget;
        char *gladefile;
	setupfeed *sf;
	
	GtkListStore  *store;
	GtkTreeIter    iter;
	int i;
	GtkCellRenderer *cell;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	g_print("rf->%p\n", rf);
	sf = g_new0(setupfeed, 1);

        gladefile = g_build_filename (EVOLUTION_GLADEDIR,
                                      "rss-ui.glade",
                                      NULL);
        sf->gui = glade_xml_new (gladefile, NULL, NULL);
        g_free (gladefile);

        GtkTreeView *treeview = (GtkTreeView *)glade_xml_get_widget (sf->gui, "feeds-treeview");
	rf->treeview = (GtkWidget *)treeview;
	sf->treeview = (GtkWidget *)treeview;

	//gtk_widget_set_size_request ((GtkWidget *)treeview, 395, -1);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

	store = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING,
                                G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), (GtkTreeModel *)store);

	cell = gtk_cell_renderer_toggle_new ();

	column = gtk_tree_view_column_new_with_attributes (_("Enabled"),
                                                  cell,
                                                  "active", 0,
                                                  NULL);
	g_signal_connect((gpointer) cell, "toggled", G_CALLBACK(enable_toggle_cb), store);
	gtk_tree_view_column_set_resizable(column, FALSE);
	gtk_tree_view_column_set_max_width (column, 70);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
                               column);
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Feed Name"),
                                                  cell,
                                                  "text", 1,
                                                  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
                               column);
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_column_clicked(column);
	column = gtk_tree_view_column_new_with_attributes (_("Type"),
                                                  cell,
                                                  "text", 2,
                                                  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
                               column);
	gtk_tree_view_column_set_sort_column_id (column, 2);
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview),
                                                   2);

	if (rf->hr != NULL)
        	g_hash_table_foreach(rf->hrname, construct_list, store);

	//make sure something (first row) is selected
       selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
       gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, 0);
       gtk_tree_selection_select_iter(selection, &iter);
 
	g_signal_connect((gpointer) treeview, 
			"row_activated", 
			G_CALLBACK(treeview_row_activated), 
			treeview);

       GtkWidget *button1 = glade_xml_get_widget (sf->gui, "feed-add-button");
       g_signal_connect(button1, "clicked", G_CALLBACK(feeds_dialog_add), treeview);

       GtkWidget *button2 = glade_xml_get_widget (sf->gui, "feed-edit-button");
       g_signal_connect(button2, "clicked", G_CALLBACK(feeds_dialog_edit), treeview);

       GtkWidget *button3 = glade_xml_get_widget (sf->gui, "feed-delete-button");
       g_signal_connect(button3, "clicked", G_CALLBACK(feeds_dialog_delete), treeview);


	rf->preferences = glade_xml_get_widget (sf->gui, "rss-config-control");
	sf->add_feed = glade_xml_get_widget (sf->gui, "add-feed-dialog");
	sf->check1 = glade_xml_get_widget(sf->gui, "checkbutton1");
	sf->check2 = glade_xml_get_widget(sf->gui, "checkbutton2");
	sf->check3 = glade_xml_get_widget(sf->gui, "checkbutton3");
	sf->spin = glade_xml_get_widget(sf->gui, "spinbutton1");

 	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->check1), 
		gconf_client_get_bool(rss_gconf, GCONF_KEY_REP_CHECK, NULL));

  	gdouble adj = gconf_client_get_float(rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT, NULL);
  	if (adj)
		gtk_spin_button_set_value((GtkSpinButton *)sf->spin, adj);
	g_signal_connect(sf->check1, "clicked", G_CALLBACK(rep_check_cb), sf->spin);
	g_signal_connect(sf->spin, "changed", G_CALLBACK(rep_check_timeout_cb), sf->check1);
	g_signal_connect(sf->spin, "value-changed", G_CALLBACK(rep_check_timeout_cb), sf->check1);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->check2),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_START_CHECK, NULL));
	g_signal_connect(sf->check2, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_START_CHECK);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->check3),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_DISPLAY_SUMMARY, NULL));
	g_signal_connect(sf->check3, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_DISPLAY_SUMMARY);


	/* HTML tab */
	sf->combo_hbox = glade_xml_get_widget(sf->gui, "hbox17");
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
        store = gtk_list_store_new(1, G_TYPE_STRING);
	GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
        for (i=0;i<3;i++) {
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, _(engines[i].label), -1);
        }
   	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
				    "text", 0,
				    NULL);
	guint render = GPOINTER_TO_INT(gconf_client_get_int(rss_gconf,
                                    GCONF_KEY_HTML_RENDER,
                                    NULL));

	switch (render)
	{
		case 10:
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
		case 1: 
#ifndef HAVE_WEBKIT
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
#endif
		case 2:
#ifndef HAVE_GTKMOZEMBED
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
#endif
		default:
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), render);

	}

	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo),
					renderer,
					set_sensitive,
					NULL, NULL);

#if !defined(HAVE_GTKMOZEMBED) && !defined (HAVE_WEBKIT)
	GtkWidget *label_webkit = glade_xml_get_widget(sf->gui, "label_webkits");
	gtk_label_set_text(label_webkit, _("Note: In order to be able to use Mozilla (Firefox) or Apple Webkit \nas renders you need firefox or webkit devel package \ninstalled and evolution-rss should be recompiled to see those packages."));
	gtk_widget_show(label_webkit);
#endif
	g_signal_connect (combo, "changed", G_CALLBACK (render_engine_changed), NULL);
	g_signal_connect (combo, "value-changed", G_CALLBACK (render_engine_changed), NULL);
	gtk_widget_show(combo);
	gtk_box_pack_start(GTK_BOX(sf->combo_hbox), combo, FALSE, FALSE, 0);

	/* Network tab */
	sf->use_proxy = glade_xml_get_widget(sf->gui, "use_proxy");
	sf->host_proxy = glade_xml_get_widget(sf->gui, "host_proxy");
	sf->port_proxy = glade_xml_get_widget(sf->gui, "port_proxy");
	sf->details = glade_xml_get_widget(sf->gui, "details");
	sf->proxy_details = glade_xml_get_widget(sf->gui, "http-proxy-details");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->use_proxy),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_USE_PROXY, NULL));
	g_signal_connect(sf->use_proxy, "clicked", G_CALLBACK(start_check_cb), GCONF_KEY_USE_PROXY);

	gchar *host = gconf_client_get_string(rss_gconf, GCONF_KEY_HOST_PROXY, NULL);
	if (host)
		gtk_entry_set_text(GTK_ENTRY(sf->host_proxy), host);
	g_signal_connect(sf->host_proxy, "changed", G_CALLBACK(host_proxy_cb), NULL);

  	gint port = gconf_client_get_int(rss_gconf, GCONF_KEY_PORT_PROXY, NULL);
  	if (port)
		gtk_spin_button_set_value((GtkSpinButton *)sf->port_proxy, (gdouble)port);
	g_signal_connect(sf->port_proxy, "changed", G_CALLBACK(port_proxy_cb), NULL);
	g_signal_connect(sf->port_proxy, "value_changed", G_CALLBACK(port_proxy_cb), NULL);

	g_signal_connect(sf->details, "clicked", G_CALLBACK(details_cb), sf->gui);


	sf->import = glade_xml_get_widget(sf->gui, "import");
	sf->export = glade_xml_get_widget(sf->gui, "export");
	g_signal_connect(sf->import, "clicked", G_CALLBACK(import_cb), sf->import);
	g_signal_connect(sf->export, "clicked", G_CALLBACK(export_cb), sf->export);

        control_widget = glade_xml_get_widget (sf->gui, "feeds-notebook");
        gtk_widget_ref (control_widget);

        gtk_container_remove (GTK_CONTAINER (control_widget->parent), control_widget);

        return evolution_config_control_new (control_widget);
}

static BonoboObject *
factory (BonoboGenericFactory *factory,
         const char *component_id,
         void *closure)
{
	g_return_val_if_fail(upgrade == 2, NULL);

	g_print("component_id:%s\n", component_id);

       if (strcmp (component_id, RSS_CONTROL_ID) == 0)
             return BONOBO_OBJECT (rss_config_control_new ());

        g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);
        return NULL;
}


BONOBO_ACTIVATION_SHLIB_FACTORY (FACTORY_ID, "Evolution RSS component factory", factory, NULL)