/**

   This code is in the public domain.  See md5.c for details.

   Authors:
     Colin Plumb [original author]
     David Helder [GNet API]

   Modified the prefix of functions to prevent conflict with original GNet.

 */


#ifndef S_GNET_MD5_H
#define S_GNET_MD5_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 *  SMD5
 *
 *  SMD5 is a MD5 hash.
 *
 **/
typedef struct _SMD5 SMD5;

/**
 *  S_GNET_MD5_HASH_LENGTH
 *
 *  Length of the MD5 hash in bytes.
 **/
#define S_GNET_MD5_HASH_LENGTH	16


SMD5*    s_gnet_md5_new (const guchar* buffer, guint length);
SMD5*	 s_gnet_md5_new_string (const gchar* str);
SMD5*    s_gnet_md5_clone (const SMD5* md5);
void     s_gnet_md5_delete (SMD5* md5);
	
SMD5*	 s_gnet_md5_new_incremental (void);
void	 s_gnet_md5_update (SMD5* md5, const guchar* buffer, guint length);
void	 s_gnet_md5_final (SMD5* md5);
	
gboolean s_gnet_md5_equal (gconstpointer p1, gconstpointer p2);
guint	 s_gnet_md5_hash (gconstpointer p);
	
gchar*   s_gnet_md5_get_digest (const SMD5* md5);
gchar*   s_gnet_md5_get_string (const SMD5* md5);
	
void	 s_gnet_md5_copy_string (const SMD5* md5, gchar* buffer);


#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif /* S_GNET_MD5_H */
