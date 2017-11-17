/*
 * LibSylph -- E-Mail client library
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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

#ifndef __CODECONV_H__
#define __CODECONV_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <iconv.h>

typedef struct _CodeConverter	CodeConverter;

typedef enum
{
	C_AUTO,
	C_US_ASCII,
	C_UTF_8,
	C_UTF_7,
	C_ISO_8859_1,
	C_ISO_8859_2,
	C_ISO_8859_3,
	C_ISO_8859_4,
	C_ISO_8859_5,
	C_ISO_8859_6,
	C_ISO_8859_7,
	C_ISO_8859_8,
	C_ISO_8859_9,
	C_ISO_8859_10,
	C_ISO_8859_11,
	C_ISO_8859_13,
	C_ISO_8859_14,
	C_ISO_8859_15,
	C_BALTIC,
	C_CP932,
	C_CP1250,
	C_CP1251,
	C_CP1252,
	C_CP1253,
	C_CP1254,
	C_CP1255,
	C_CP1256,
	C_CP1257,
	C_CP1258,
	C_WINDOWS_932,
	C_WINDOWS_1250,
	C_WINDOWS_1251,
	C_WINDOWS_1252,
	C_WINDOWS_1253,
	C_WINDOWS_1254,
	C_WINDOWS_1255,
	C_WINDOWS_1256,
	C_WINDOWS_1257,
	C_WINDOWS_1258,
	C_KOI8_R,
	C_KOI8_T,
	C_KOI8_U,
	C_ISO_2022_JP,
	C_ISO_2022_JP_2,
	C_ISO_2022_JP_3,
	C_EUC_JP,
	C_EUC_JP_MS,
	C_SHIFT_JIS,
	C_ISO_2022_KR,
	C_EUC_KR,
	C_ISO_2022_CN,
	C_EUC_CN,
	C_GB2312,
	C_GBK,
	C_EUC_TW,
	C_BIG5,
	C_BIG5_HKSCS,
	C_TIS_620,
	C_WINDOWS_874,
	C_GEORGIAN_PS,
	C_TCVN5712_1,
	C_ISO_8859_16,
	C_UTF_16,
	C_UTF_16BE,
	C_UTF_16LE
} CharSet;

typedef enum
{
	C_AD_BY_LOCALE,
	C_AD_NEVER,
	C_AD_JAPANESE
} ConvADType;

typedef gchar *(*CodeConvFunc) (const gchar *inbuf, gint *error);

struct _CodeConverter
{
	CodeConvFunc code_conv_func;
	gchar *src_encoding;
	gchar *dest_encoding;
};

#define CS_AUTO			"AUTO"
#define CS_US_ASCII		"US-ASCII"
#define CS_ANSI_X3_4_1968	"ANSI_X3.4-1968"
#define CS_UTF_8		"UTF-8"
#define CS_UTF_7		"UTF-7"
#define CS_ISO_8859_1		"ISO-8859-1"
#define CS_ISO_8859_2		"ISO-8859-2"
#define CS_ISO_8859_3		"ISO-8859-3"
#define CS_ISO_8859_4		"ISO-8859-4"
#define CS_ISO_8859_5		"ISO-8859-5"
#define CS_ISO_8859_6		"ISO-8859-6"
#define CS_ISO_8859_7		"ISO-8859-7"
#define CS_ISO_8859_8		"ISO-8859-8"
#define CS_ISO_8859_9		"ISO-8859-9"
#define CS_ISO_8859_10		"ISO-8859-10"
#define CS_ISO_8859_11		"ISO-8859-11"
#define CS_ISO_8859_13		"ISO-8859-13"
#define CS_ISO_8859_14		"ISO-8859-14"
#define CS_ISO_8859_15		"ISO-8859-15"
#define CS_BALTIC		"BALTIC"
#define CS_CP932		"CP932"
#define CS_CP1250		"CP1250"
#define CS_CP1251		"CP1251"
#define CS_CP1252		"CP1252"
#define CS_CP1253		"CP1253"
#define CS_CP1254		"CP1254"
#define CS_CP1255		"CP1255"
#define CS_CP1256		"CP1256"
#define CS_CP1257		"CP1257"
#define CS_CP1258		"CP1258"
#define CS_WINDOWS_932		"Windows-932"
#define CS_WINDOWS_1250		"Windows-1250"
#define CS_WINDOWS_1251		"Windows-1251"
#define CS_WINDOWS_1252		"Windows-1252"
#define CS_WINDOWS_1253		"Windows-1253"
#define CS_WINDOWS_1254		"Windows-1254"
#define CS_WINDOWS_1255		"Windows-1255"
#define CS_WINDOWS_1256		"Windows-1256"
#define CS_WINDOWS_1257		"Windows-1257"
#define CS_WINDOWS_1258		"Windows-1258"
#define CS_KOI8_R		"KOI8-R"
#define CS_KOI8_T		"KOI8-T"
#define CS_KOI8_U		"KOI8-U"
#define CS_ISO_2022_JP		"ISO-2022-JP"
#define CS_ISO_2022_JP_2	"ISO-2022-JP-2"
#define CS_ISO_2022_JP_3	"ISO-2022-JP-3"
#define CS_EUC_JP		"EUC-JP"
#define CS_EUCJP		"EUCJP"
#define CS_EUC_JP_MS		"EUC-JP-MS"
#define CS_SHIFT_JIS		"Shift_JIS"
#define CS_SHIFT__JIS		"SHIFT-JIS"
#define CS_SJIS			"SJIS"
#define CS_X_SJIS		"X-SJIS"
#define CS_ISO_2022_KR		"ISO-2022-KR"
#define CS_EUC_KR		"EUC-KR"
#define CS_KS_C_5601_1987	"ks_c_5601-1987"
#define CS_ISO_2022_CN		"ISO-2022-CN"
#define CS_EUC_CN		"EUC-CN"
#define CS_GB2312		"GB2312"
#define CS_GBK			"GBK"
#define CS_X_GBK		"X-GBK"
#define CS_EUC_TW		"EUC-TW"
#define CS_BIG5			"Big5"
#define CS_BIG5_HKSCS		"BIG5-HKSCS"
#define CS_TIS_620		"TIS-620"
#define CS_WINDOWS_874		"Windows-874"
#define CS_GEORGIAN_PS		"GEORGIAN-PS"
#define CS_TCVN5712_1		"TCVN5712-1"
#define CS_ISO_8859_16		"ISO-8859-16"
#define CS_UTF_16		"UTF-16"
#define CS_UTF_16BE		"UTF-16BE"
#define CS_UTF_16LE		"UTF-16LE"

#define C_INTERNAL		C_UTF_8
#define CS_INTERNAL		CS_UTF_8

//void conv_mb_alnum(gchar *str);

CharSet conv_guess_ja_encoding		(const gchar	*str);

gchar *conv_utf8todisp			(const gchar	*inbuf,
					 gint		*error);
gchar *conv_localetodisp		(const gchar	*inbuf,
					 gint		*error);

CodeConverter *conv_code_converter_new	(const gchar	*src_encoding,
					 const gchar	*dest_encoding);
void conv_code_converter_destroy	(CodeConverter	*conv);
gchar *conv_convert			(CodeConverter	*conv,
					 const gchar	*inbuf);

#define conv_codeset_strdup(inbuf, src_code, dest_code) \
	(conv_codeset_strdup_full(inbuf, src_code, dest_code, NULL))

gchar *conv_codeset_strdup_full		(const gchar	*inbuf,
					 const gchar	*src_encoding,
					 const gchar	*dest_encoding,
					 gint		*error);

CodeConvFunc conv_get_code_conv_func	(const gchar	*src_encoding,
					 const gchar	*dest_encoding);

gchar *conv_iconv_strdup		(const gchar	*inbuf,
					 const gchar	*src_encoding,
					 const gchar	*dest_encoding,
					 gint		*error);
gchar *conv_iconv_strdup_with_cd	(const gchar	*inbuf,
					 iconv_t	 cd,
					 gint		*error);

const gchar *conv_get_charset_str		(CharSet	 charset);
CharSet conv_get_charset_from_str		(const gchar	*charset);
CharSet conv_get_locale_charset			(void);
const gchar *conv_get_locale_charset_str	(void);
CharSet conv_get_internal_charset		(void);
const gchar *conv_get_internal_charset_str	(void);
CharSet conv_get_outgoing_charset		(void);
const gchar *conv_get_outgoing_charset_str	(void);
gboolean conv_is_multibyte_encoding		(CharSet	 encoding);

const gchar *conv_get_current_locale		(void);
gboolean conv_is_ja_locale			(void);

void conv_set_autodetect_type			(ConvADType	 type);
ConvADType conv_get_autodetect_type		(void);

gchar *conv_unmime_header		(const gchar	*str,
					 const gchar	*default_encoding);
void conv_encode_header			(gchar		*dest,
					 gint		 len,
					 const gchar	*src,
					 gint		 header_len,
					 gboolean	 addr_field,
					 const gchar	*out_encoding);
gchar *conv_encode_filename		(const gchar	*src,
					 const gchar	*param_name,
					 const gchar	*out_encoding);

gint conv_copy_file			(const gchar	*src,
					 const gchar	*dest,
					 const gchar	*src_encoding);
gint conv_copy_dir			(const gchar	*src,
					 const gchar	*dest,
					 const gchar	*src_encoding);

CharSet conv_check_file_encoding	(const gchar	*file);

gchar *conv_filename_from_utf8		(const gchar	*utf8_file);
gchar *conv_filename_to_utf8		(const gchar	*fs_file);

#endif /* __CODECONV_H__ */
