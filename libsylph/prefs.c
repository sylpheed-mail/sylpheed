/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef G_OS_WIN32
#  include <windows.h>
#  include <io.h>
#endif

#include "prefs.h"
#include "codeconv.h"
#include "utils.h"

typedef enum
{
	DUMMY_PARAM
} DummyEnum;

typedef struct _PrefFilePrivate	PrefFilePrivate;

struct _PrefFilePrivate {
	FILE *fp;
	gchar *path;
	gint backup_generation;
};

static void prefs_config_parse_one_line	(GHashTable	*param_table,
					 const gchar	*buf);

GHashTable *prefs_param_table_get(PrefParam *param)
{
	GHashTable *table;
	gint i;

	g_return_val_if_fail(param != NULL, NULL);

	table = g_hash_table_new(g_str_hash, g_str_equal);

	for (i = 0; param[i].name != NULL; i++) {
		g_hash_table_insert(table, param[i].name, &param[i]);
	}

	return table;
}

void prefs_param_table_destroy(GHashTable *param_table)
{
	g_hash_table_destroy(param_table);
}

void prefs_read_config(PrefParam *param, const gchar *label,
		       const gchar *rcfile, const gchar *encoding)
{
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	gchar *block_label;
	GHashTable *param_table;

	g_return_if_fail(param != NULL);
	g_return_if_fail(label != NULL);
	g_return_if_fail(rcfile != NULL);

	debug_print("Reading configuration...\n");

	prefs_set_default(param);

	if ((fp = g_fopen(rcfile, "rb")) == NULL) {
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
				(buf, encoding, CS_INTERNAL);
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

	param_table = prefs_param_table_get(param);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (buf[0] == '\0') continue;
		/* reached next block */
		if (buf[0] == '[') break;

		if (encoding) {
			gchar *conv_str;

			conv_str = conv_codeset_strdup
				(buf, encoding, CS_INTERNAL);
			if (!conv_str)
				conv_str = g_strdup(buf);
			prefs_config_parse_one_line(param_table, conv_str);
			g_free(conv_str);
		} else
			prefs_config_parse_one_line(param_table, buf);
	}

	prefs_param_table_destroy(param_table);

	debug_print("Finished reading configuration.\n");
	fclose(fp);
}

static void prefs_config_parse_one_line(GHashTable *param_table,
					const gchar *buf)
{
	PrefParam *param;
	const gchar *p = buf;
	gchar *name;
	const gchar *value;

	while (*p && *p != '=')
		p++;

	if (*p != '=') {
		g_warning("invalid pref line: %s\n", buf);
		return;
	}

	name = g_strndup(buf, p - buf);
	value = p + 1;

	/* debug_print("%s = %s\n", name, value); */

	param = g_hash_table_lookup(param_table, name);

	if (!param) {
		debug_print("pref key '%s' (value '%s') not found\n",
			    name, value);
		g_free(name);
		return;
	}

	switch (param->type) {
	case P_STRING:
		g_free(*((gchar **)param->data));
		*((gchar **)param->data) = *value ? g_strdup(value) : NULL;
		break;
	case P_INT:
		*((gint *)param->data) = (gint)atoi(value);
		break;
	case P_BOOL:
		*((gboolean *)param->data) =
			(*value == '0' || *value == '\0') ? FALSE : TRUE;
		break;
	case P_ENUM:
		*((DummyEnum *)param->data) = (DummyEnum)atoi(value);
		break;
	case P_USHORT:
		*((gushort *)param->data) = (gushort)atoi(value);
		break;
	default:
		break;
	}

	g_free(name);
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
	if ((orig_fp = g_fopen(rcpath, "rb")) == NULL) {
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
	PrefFilePrivate *pfile;
	gchar *tmppath;
	FILE *fp;

	g_return_val_if_fail(path != NULL, NULL);

	tmppath = g_strconcat(path, ".tmp", NULL);
	if ((fp = g_fopen(tmppath, "wb")) == NULL) {
		FILE_OP_ERROR(tmppath, "fopen");
		g_free(tmppath);
		return NULL;
	}

	if (change_file_mode_rw(fp, tmppath) < 0)
		FILE_OP_ERROR(tmppath, "chmod");

	g_free(tmppath);

	pfile = g_new(PrefFilePrivate, 1);
	pfile->fp = fp;
	pfile->path = g_strdup(path);
	pfile->backup_generation = 4;

	return (PrefFile *)pfile;
}

void prefs_file_set_backup_generation(PrefFile *pfile, gint generation)
{
	PrefFilePrivate *priv = (PrefFilePrivate *)pfile;

	g_return_if_fail(pfile != NULL);

	priv->backup_generation = generation;
}

gint prefs_file_get_backup_generation(PrefFile *pfile)
{
	PrefFilePrivate *priv = (PrefFilePrivate *)pfile;

	g_return_val_if_fail(pfile != NULL, -1);

	return priv->backup_generation;
}

gint prefs_file_close(PrefFile *pfile)
{
	PrefFilePrivate *priv = (PrefFilePrivate *)pfile;
	FILE *fp;
	gchar *path;
	gchar *tmppath;
	gchar *bakpath = NULL;
	gint generation;
	gint ret = 0;

	g_return_val_if_fail(pfile != NULL, -1);

	fp = pfile->fp;
	path = pfile->path;
	generation = priv->backup_generation;
	g_free(pfile);

	tmppath = g_strconcat(path, ".tmp", NULL);
	if (fflush(fp) == EOF) {
		FILE_OP_ERROR(tmppath, "fflush");
		fclose(fp);
		ret = -1;
		goto finish;
	}
#if HAVE_FSYNC
	if (fsync(fileno(fp)) < 0) {
		FILE_OP_ERROR(tmppath, "fsync");
		fclose(fp);
		ret = -1;
		goto finish;
	}
#elif defined(G_OS_WIN32)
	if (_commit(_fileno(fp)) < 0) {
		FILE_OP_ERROR(tmppath, "_commit");
		fclose(fp);
		ret = -1;
		goto finish;
	}
#endif
	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmppath, "fclose");
		ret = -1;
		goto finish;
	}

	if (is_file_exist(path)) {
		bakpath = g_strconcat(path, ".bak", NULL);
		if (is_file_exist(bakpath)) {
			gint i;
			gchar *bakpath_n, *bakpath_p;

			for (i = generation; i > 0; i--) {
				bakpath_n = g_strdup_printf("%s.%d", bakpath,
							    i);
				if (i == 1)
					bakpath_p = g_strdup(bakpath);
				else
					bakpath_p = g_strdup_printf
						("%s.%d", bakpath, i - 1);
				if (is_file_exist(bakpath_p)) {
					if (rename_force(bakpath_p, bakpath_n) < 0) {
						FILE_OP_ERROR(bakpath_p,
							      "rename");
					}
				}
				g_free(bakpath_p);
				g_free(bakpath_n);
			}
		}
		if (rename_force(path, bakpath) < 0) {
			FILE_OP_ERROR(path, "rename");
			ret = -1;
			goto finish;
		}
	}

	if (rename_force(tmppath, path) < 0) {
		FILE_OP_ERROR(tmppath, "rename");
		ret = -1;
		goto finish;
	}

finish:
	if (ret < 0)
		g_unlink(tmppath);
	g_free(path);
	g_free(tmppath);
	g_free(bakpath);
	return ret;
}

gint prefs_file_close_revert(PrefFile *pfile)
{
	gchar *tmppath;

	g_return_val_if_fail(pfile != NULL, -1);

	tmppath = g_strconcat(pfile->path, ".tmp", NULL);
	fclose(pfile->fp);
	if (g_unlink(tmppath) < 0)
		FILE_OP_ERROR(tmppath, "unlink");
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
				if (!g_ascii_strncasecmp(param[i].defval, "ENV_", 4)) {
					const gchar *envstr;
					gchar *tmp = NULL;

					envstr = g_getenv(param[i].defval + 4);
#ifdef G_OS_WIN32
					tmp = g_strdup(envstr);
#else
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
#endif
					*((gchar **)param[i].data) = tmp;
				} else if (param[i].defval[0] == '~')
					*((gchar **)param[i].data) =
#ifdef G_OS_WIN32
						g_strconcat(get_rc_dir(),
#else
						g_strconcat(get_home_dir(),
#endif
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
				if (!g_ascii_strcasecmp(param[i].defval, "TRUE"))
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
