/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"

typedef enum
{
	DUMMY_PARAM
} DummyEnum;

void prefs_read_config(PrefParam *param, const gchar *label,
		       const gchar *rcfile, const gchar *encoding)
{
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	gchar *block_label;

	g_return_if_fail(param != NULL);
	g_return_if_fail(label != NULL);
	g_return_if_fail(rcfile != NULL);

	debug_print("Reading configuration...\n");

	prefs_set_default(param);

	if ((fp = fopen(rcfile, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcfile, "fopen");
		return;
	}

	block_label = g_strdup_printf("[%s]", label);

	/* search aiming block */
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gint val;

		if (encoding) {
			gchar *conv_str;

			conv_str = conv_codeset_strdup
				(buf, encoding,
				 conv_get_internal_charset_str());
			if (!conv_str)
				conv_str = g_strdup(buf);
			val = strncmp
				(conv_str, block_label, strlen(block_label));
			g_free(conv_str);
		} else
			val = strncmp(buf, block_label, strlen(block_label));
		if (val == 0) {
			debug_print("Found %s\n", block_label);
			break;
		}
	}
	g_free(block_label);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		/* reached next block */
		if (buf[0] == '[') break;

		if (encoding) {
			gchar *conv_str;

			conv_str = conv_codeset_strdup
				(buf, encoding,
				 conv_get_internal_charset_str());
			if (!conv_str)
				conv_str = g_strdup(buf);
			prefs_config_parse_one_line(param, conv_str);
			g_free(conv_str);
		} else
			prefs_config_parse_one_line(param, buf);
	}

	debug_print("Finished reading configuration.\n");
	fclose(fp);
}

void prefs_config_parse_one_line(PrefParam *param, const gchar *buf)
{
	gint i;
	gint name_len;
	const gchar *value;

	for (i = 0; param[i].name != NULL; i++) {
		name_len = strlen(param[i].name);
		if (strncasecmp(buf, param[i].name, name_len))
			continue;
		if (buf[name_len] != '=')
			continue;
		value = buf + name_len + 1;
		/* debug_print("%s = %s\n", param[i].name, value); */

		switch (param[i].type) {
		case P_STRING:
			g_free(*((gchar **)param[i].data));
			*((gchar **)param[i].data) =
				*value ? g_strdup(value) : NULL;
			break;
		case P_INT:
			*((gint *)param[i].data) =
				(gint)atoi(value);
			break;
		case P_BOOL:
			*((gboolean *)param[i].data) =
				(*value == '0' || *value == '\0')
					? FALSE : TRUE;
			break;
		case P_ENUM:
			*((DummyEnum *)param[i].data) =
				(DummyEnum)atoi(value);
			break;
		case P_USHORT:
			*((gushort *)param[i].data) =
				(gushort)atoi(value);
			break;
		default:
			break;
		}
	}
}

#define TRY(func) \
if (!(func)) \
{ \
	g_warning(_("failed to write configuration to file\n")); \
	if (orig_fp) fclose(orig_fp); \
	prefs_file_close_revert(pfile); \
	g_free(rcpath); \
	g_free(block_label); \
	return; \
} \

void prefs_write_config(PrefParam *param, const gchar *label,
			const gchar *rcfile)
{
	FILE *orig_fp;
	PrefFile *pfile;
	gchar *rcpath;
	gchar buf[PREFSBUFSIZE];
	gchar *block_label = NULL;
	gboolean block_matched = FALSE;

	g_return_if_fail(param != NULL);
	g_return_if_fail(label != NULL);
	g_return_if_fail(rcfile != NULL);

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, rcfile, NULL);
	if ((orig_fp = fopen(rcpath, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
	}

	if ((pfile = prefs_file_open(rcpath)) == NULL) {
		g_warning(_("failed to write configuration to file\n"));
		if (orig_fp) fclose(orig_fp);
		g_free(rcpath);
		return;
	}

	block_label = g_strdup_printf("[%s]", label);

	/* search aiming block */
	if (orig_fp) {
		while (fgets(buf, sizeof(buf), orig_fp) != NULL) {
			gint val;

			val = strncmp(buf, block_label, strlen(block_label));
			if (val == 0) {
				debug_print(_("Found %s\n"), block_label);
				block_matched = TRUE;
				break;
			} else
				TRY(fputs(buf, pfile->fp) != EOF);
		}
	}

	TRY(fprintf(pfile->fp, "%s\n", block_label) > 0);
	g_free(block_label);
	block_label = NULL;

	/* write all param data to file */
	TRY(prefs_file_write_param(pfile, param) == 0);

	if (block_matched) {
		while (fgets(buf, sizeof(buf), orig_fp) != NULL) {
			/* next block */
			if (buf[0] == '[') {
				TRY(fputc('\n', pfile->fp) != EOF &&
				    fputs(buf, pfile->fp)  != EOF);
				break;
			}
		}
		while (fgets(buf, sizeof(buf), orig_fp) != NULL)
			TRY(fputs(buf, pfile->fp) != EOF);
	}

	if (orig_fp) fclose(orig_fp);
	if (prefs_file_close(pfile) < 0)
		g_warning(_("failed to write configuration to file\n"));
	g_free(rcpath);

	debug_print(_("Configuration is saved.\n"));
}

gint prefs_file_write_param(PrefFile *pfile, PrefParam *param)
{
	gint i;
	gchar buf[PREFSBUFSIZE];

	for (i = 0; param[i].name != NULL; i++) {
		switch (param[i].type) {
		case P_STRING:
			g_snprintf(buf, sizeof(buf), "%s=%s\n", param[i].name,
				   *((gchar **)param[i].data) ?
				   *((gchar **)param[i].data) : "");
			break;
		case P_INT:
			g_snprintf(buf, sizeof(buf), "%s=%d\n", param[i].name,
				   *((gint *)param[i].data));
			break;
		case P_BOOL:
			g_snprintf(buf, sizeof(buf), "%s=%d\n", param[i].name,
				   *((gboolean *)param[i].data));
			break;
		case P_ENUM:
			g_snprintf(buf, sizeof(buf), "%s=%d\n", param[i].name,
				   *((DummyEnum *)param[i].data));
			break;
		case P_USHORT:
			g_snprintf(buf, sizeof(buf), "%s=%d\n", param[i].name,
				   *((gushort *)param[i].data));
			break;
		default:
			buf[0] = '\0';
		}

		if (buf[0] != '\0') {
			if (fputs(buf, pfile->fp) == EOF) {
				perror("fputs");
				return -1;
			}
		}
	}

	return 0;
}

PrefFile *prefs_file_open(const gchar *path)
{
	PrefFile *pfile;
	gchar *tmppath;
	FILE *fp;

	g_return_val_if_fail(path != NULL, NULL);

	tmppath = g_strconcat(path, ".tmp", NULL);
	if ((fp = fopen(tmppath, "wb")) == NULL) {
		FILE_OP_ERROR(tmppath, "fopen");
		g_free(tmppath);
		return NULL;
	}

	if (change_file_mode_rw(fp, tmppath) < 0)
		FILE_OP_ERROR(tmppath, "chmod");

	g_free(tmppath);

	pfile = g_new(PrefFile, 1);
	pfile->fp = fp;
	pfile->path = g_strdup(path);

	return pfile;
}

gint prefs_file_close(PrefFile *pfile)
{
	FILE *fp;
	gchar *path;
	gchar *tmppath;
	gchar *bakpath = NULL;

	g_return_val_if_fail(pfile != NULL, -1);

	fp = pfile->fp;
	path = pfile->path;
	g_free(pfile);

	tmppath = g_strconcat(path, ".tmp", NULL);
	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmppath, "fclose");
		unlink(tmppath);
		g_free(path);
		g_free(tmppath);
		return -1;
	}

	if (is_file_exist(path)) {
		bakpath = g_strconcat(path, ".bak", NULL);
		if (rename(path, bakpath) < 0) {
			FILE_OP_ERROR(path, "rename");
			unlink(tmppath);
			g_free(path);
			g_free(tmppath);
			g_free(bakpath);
			return -1;
		}
	}

	if (rename(tmppath, path) < 0) {
		FILE_OP_ERROR(tmppath, "rename");
		unlink(tmppath);
		g_free(path);
		g_free(tmppath);
		g_free(bakpath);
		return -1;
	}

	g_free(path);
	g_free(tmppath);
	g_free(bakpath);
	return 0;
}

gint prefs_file_close_revert(PrefFile *pfile)
{
	gchar *tmppath;

	g_return_val_if_fail(pfile != NULL, -1);

	tmppath = g_strconcat(pfile->path, ".tmp", NULL);
	fclose(pfile->fp);
	if (unlink(tmppath) < 0) FILE_OP_ERROR(tmppath, "unlink");
	g_free(tmppath);
	g_free(pfile->path);
	g_free(pfile);

	return 0;
}

void prefs_set_default(PrefParam *param)
{
	gint i;

	g_return_if_fail(param != NULL);

	for (i = 0; param[i].name != NULL; i++) {
		if (!param[i].data) continue;

		switch (param[i].type) {
		case P_STRING:
			if (param[i].defval != NULL) {
				if (!strncasecmp(param[i].defval, "ENV_", 4)) {
					const gchar *envstr;
					gchar *tmp = NULL;

					envstr = g_getenv(param[i].defval + 4);
					if (envstr) {
						tmp = conv_codeset_strdup
							(envstr,
							 conv_get_locale_charset_str(),
							 CS_UTF_8);
						if (!tmp) {
							g_warning("failed to convert character set.");
							tmp = g_strdup(envstr);
						}
					}
					*((gchar **)param[i].data) = tmp;
				} else if (param[i].defval[0] == '~')
					*((gchar **)param[i].data) =
						g_strconcat(get_home_dir(),
							    param[i].defval + 1,
							    NULL);
				else if (param[i].defval[0] != '\0')
					*((gchar **)param[i].data) =
						g_strdup(param[i].defval);
				else
					*((gchar **)param[i].data) = NULL;
			} else
				*((gchar **)param[i].data) = NULL;
			break;
		case P_INT:
			if (param[i].defval != NULL)
				*((gint *)param[i].data) =
					(gint)atoi(param[i].defval);
			else
				*((gint *)param[i].data) = 0;
			break;
		case P_BOOL:
			if (param[i].defval != NULL) {
				if (!strcasecmp(param[i].defval, "TRUE"))
					*((gboolean *)param[i].data) = TRUE;
				else
					*((gboolean *)param[i].data) =
						atoi(param[i].defval) ? TRUE : FALSE;
			} else
				*((gboolean *)param[i].data) = FALSE;
			break;
		case P_ENUM:
			if (param[i].defval != NULL)
				*((DummyEnum*)param[i].data) =
					(DummyEnum)atoi(param[i].defval);
			else
				*((DummyEnum *)param[i].data) = 0;
			break;
		case P_USHORT:
			if (param[i].defval != NULL)
				*((gushort *)param[i].data) =
					(gushort)atoi(param[i].defval);
			else
				*((gushort *)param[i].data) = 0;
			break;
		default:
			break;
		}
	}
}

void prefs_free(PrefParam *param)
{
	gint i;

	g_return_if_fail(param != NULL);

	for (i = 0; param[i].name != NULL; i++) {
		if (!param[i].data) continue;

		switch (param[i].type) {
		case P_STRING:
			g_free(*((gchar **)param[i].data));
			break;
		default:
			break;
		}
	}
}

void prefs_dialog_create(PrefsDialog *dialog)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *notebook;

	GtkWidget *confirm_area;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *apply_btn;

	g_return_if_fail(dialog != NULL);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW(window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	notebook = gtk_notebook_new ();
	gtk_widget_show(notebook);
	gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 2);
	/* GTK_WIDGET_UNSET_FLAGS (notebook, GTK_CAN_FOCUS); */
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);

	gtkut_button_set_create(&confirm_area,
				&ok_btn,	_("OK"),
				&cancel_btn,	_("Cancel"),
				&apply_btn,	_("Apply"));
	gtk_widget_show(confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	dialog->window     = window;
	dialog->notebook   = notebook;
	dialog->ok_btn     = ok_btn;
	dialog->cancel_btn = cancel_btn;
	dialog->apply_btn  = apply_btn;
}

void prefs_dialog_destroy(PrefsDialog *dialog)
{
	gtk_widget_destroy(dialog->window);
	dialog->window     = NULL;
	dialog->notebook   = NULL;
	dialog->ok_btn     = NULL;
	dialog->cancel_btn = NULL;
	dialog->apply_btn  = NULL;
}

void prefs_button_toggled(GtkToggleButton *toggle_btn, GtkWidget *widget)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active(toggle_btn);
	gtk_widget_set_sensitive(widget, is_active);
}

void prefs_set_dialog(PrefParam *param)
{
	gint i;

	for (i = 0; param[i].name != NULL; i++) {
		if (param[i].widget_set_func)
			param[i].widget_set_func(&param[i]);
	}
}

void prefs_set_data_from_dialog(PrefParam *param)
{
	gint i;

	for (i = 0; param[i].name != NULL; i++) {
		if (param[i].data_set_func)
			param[i].data_set_func(&param[i]);
	}
}

void prefs_set_dialog_to_default(PrefParam *param)
{
	gint	   i;
	PrefParam  tmpparam;
	gchar	  *str_data = NULL;
	gint	   int_data;
	gushort    ushort_data;
	gboolean   bool_data;
	DummyEnum  enum_data;

	for (i = 0; param[i].name != NULL; i++) {
		if (!param[i].widget_set_func) continue;

		tmpparam = param[i];

		switch (tmpparam.type) {
		case P_STRING:
#warning FIXME_GTK2
			if (tmpparam.defval) {
				if (!strncasecmp(tmpparam.defval, "ENV_", 4)) {
					str_data = g_strdup(g_getenv(param[i].defval + 4));
					tmpparam.data = &str_data;
					break;
				} else if (tmpparam.defval[0] == '~') {
					str_data =
						g_strconcat(get_home_dir(),
							    param[i].defval + 1,
							    NULL);
					tmpparam.data = &str_data;
					break;
				}
			}
			tmpparam.data = &tmpparam.defval;
			break;
		case P_INT:
			if (tmpparam.defval)
				int_data = atoi(tmpparam.defval);
			else
				int_data = 0;
			tmpparam.data = &int_data;
			break;
		case P_USHORT:
			if (tmpparam.defval)
				ushort_data = atoi(tmpparam.defval);
			else
				ushort_data = 0;
			tmpparam.data = &ushort_data;
			break;
		case P_BOOL:
			if (tmpparam.defval) {
				if (!strcasecmp(tmpparam.defval, "TRUE"))
					bool_data = TRUE;
				else
					bool_data = atoi(tmpparam.defval)
						? TRUE : FALSE;
			} else
				bool_data = FALSE;
			tmpparam.data = &bool_data;
			break;
		case P_ENUM:
			if (tmpparam.defval)
				enum_data = (DummyEnum)atoi(tmpparam.defval);
			else
				enum_data = 0;
			tmpparam.data = &enum_data;
			break;
		case P_OTHER:
			break;
		}
		tmpparam.widget_set_func(&tmpparam);
		g_free(str_data);
		str_data = NULL;
	}
}

void prefs_set_data_from_entry(PrefParam *pparam)
{
	gchar **str;
	const gchar *entry_str;

	g_return_if_fail(*pparam->widget != NULL);

	entry_str = gtk_entry_get_text(GTK_ENTRY(*pparam->widget));

	switch (pparam->type) {
	case P_STRING:
		str = (gchar **)pparam->data;
		g_free(*str);
		*str = entry_str[0] ? g_strdup(entry_str) : NULL;
		break;
	case P_USHORT:
		*((gushort *)pparam->data) = atoi(entry_str);
		break;
	case P_INT:
		*((gint *)pparam->data) = atoi(entry_str);
		break;
	default:
		g_warning("Invalid PrefType for GtkEntry widget: %d\n",
			  pparam->type);
	}
}

void prefs_set_entry(PrefParam *pparam)
{
	gchar **str;

	g_return_if_fail(*pparam->widget != NULL);

	switch (pparam->type) {
	case P_STRING:
		str = (gchar **)pparam->data;
		gtk_entry_set_text(GTK_ENTRY(*pparam->widget),
				   *str ? *str : "");
		break;
	case P_INT:
		gtk_entry_set_text(GTK_ENTRY(*pparam->widget),
				   itos(*((gint *)pparam->data)));
		break;
	case P_USHORT:
		gtk_entry_set_text(GTK_ENTRY(*pparam->widget),
				   itos(*((gushort *)pparam->data)));
		break;
	default:
		g_warning("Invalid PrefType for GtkEntry widget: %d\n",
			  pparam->type);
	}
}

void prefs_set_data_from_text(PrefParam *pparam)
{
	gchar **str;
	gchar *text = NULL, *tp = NULL;
	gchar *tmp, *tmpp;

	g_return_if_fail(*pparam->widget != NULL);
	g_return_if_fail(GTK_IS_EDITABLE(*pparam->widget) ||
			 GTK_IS_TEXT_VIEW(*pparam->widget));

	switch (pparam->type) {
	case P_STRING:
		str = (gchar **)pparam->data;
		g_free(*str);
		if (GTK_IS_EDITABLE(*pparam->widget)) {
			tp = text = gtk_editable_get_chars
				(GTK_EDITABLE(*pparam->widget), 0, -1);
		} else if (GTK_IS_TEXT_VIEW(*pparam->widget)) {
			GtkTextView *textview = GTK_TEXT_VIEW(*pparam->widget);
			GtkTextBuffer *buffer;
			GtkTextIter start, end;

			buffer = gtk_text_view_get_buffer(textview);
			gtk_text_buffer_get_start_iter(buffer, &start);
			gtk_text_buffer_get_iter_at_offset(buffer, &end, -1);
			tp = text = gtk_text_buffer_get_text
				(buffer, &start, &end, FALSE);
		}

		g_return_if_fail(tp != NULL && text != NULL);

		if (text[0] == '\0') {
			*str = NULL;
			g_free(text);
			break;
		}

		Xalloca(tmpp = tmp, strlen(text) * 2 + 1,
			{ *str = NULL; break; });
		while (*tp) {
			if (*tp == '\n') {
				*tmpp++ = '\\';
				*tmpp++ = 'n';
				tp++;
			} else
				*tmpp++ = *tp++;
		}
		*tmpp = '\0';
		*str = g_strdup(tmp);
		g_free(text);
		break;
	default:
		g_warning("Invalid PrefType for GtkTextView widget: %d\n",
			  pparam->type);
	}
}

void prefs_set_text(PrefParam *pparam)
{
	gchar *buf, *sp, *bufp;
	gchar **str;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	g_return_if_fail(*pparam->widget != NULL);

	switch (pparam->type) {
	case P_STRING:
		str = (gchar **)pparam->data;
		if (*str) {
			bufp = buf = alloca(strlen(*str) + 1);
			if (!buf) buf = "";
			else {
				sp = *str;
				while (*sp) {
					if (*sp == '\\' && *(sp + 1) == 'n') {
						*bufp++ = '\n';
						sp += 2;
					} else
						*bufp++ = *sp++;
				}
				*bufp = '\0';
			}
		} else
			buf = "";

		text = GTK_TEXT_VIEW(*pparam->widget);
		buffer = gtk_text_view_get_buffer(text);
		gtk_text_buffer_set_text(buffer, "", 0);
		gtk_text_buffer_get_start_iter(buffer, &iter);
		gtk_text_buffer_insert(buffer, &iter, buf, -1);
		break;
	default:
		g_warning("Invalid PrefType for GtkTextView widget: %d\n",
			  pparam->type);
	}
}

void prefs_set_data_from_toggle(PrefParam *pparam)
{
	g_return_if_fail(pparam->type == P_BOOL);
	g_return_if_fail(*pparam->widget != NULL);
	
	*((gboolean *)pparam->data) =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(*pparam->widget));
}

void prefs_set_toggle(PrefParam *pparam)
{
	g_return_if_fail(pparam->type == P_BOOL);
	g_return_if_fail(*pparam->widget != NULL);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(*pparam->widget),
				     *((gboolean *)pparam->data));
}

void prefs_set_data_from_spinbtn(PrefParam *pparam)
{
	g_return_if_fail(*pparam->widget != NULL);

	switch (pparam->type) {
	case P_INT:
		*((gint *)pparam->data) =
			gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(*pparam->widget));
		break;
	case P_USHORT:
		*((gushort *)pparam->data) =
			(gushort)gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(*pparam->widget));
		break;
	default:
		g_warning("Invalid PrefType for GtkSpinButton widget: %d\n",
			  pparam->type);
	}
}

void prefs_set_spinbtn(PrefParam *pparam)
{
	g_return_if_fail(*pparam->widget != NULL);

	switch (pparam->type) {
	case P_INT:
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(*pparam->widget),
					  (gfloat)*((gint *)pparam->data));
		break;
	case P_USHORT:
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(*pparam->widget),
					  (gfloat)*((gushort *)pparam->data));
		break;
	default:
		g_warning("Invalid PrefType for GtkSpinButton widget: %d\n",
			  pparam->type);
	}
}
