
#ifndef __syl_marshal_MARSHAL_H__
#define __syl_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:VOID (syl-marshal.list:1) */
#define syl_marshal_VOID__VOID	g_cclosure_marshal_VOID__VOID

/* VOID:POINTER,STRING,UINT (syl-marshal.list:2) */
extern void syl_marshal_VOID__POINTER_STRING_UINT (GClosure     *closure,
                                                   GValue       *return_value,
                                                   guint         n_param_values,
                                                   const GValue *param_values,
                                                   gpointer      invocation_hint,
                                                   gpointer      marshal_data);

/* VOID:POINTER (syl-marshal.list:3) */
#define syl_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

/* VOID:POINTER,STRING,STRING (syl-marshal.list:4) */
extern void syl_marshal_VOID__POINTER_STRING_STRING (GClosure     *closure,
                                                     GValue       *return_value,
                                                     guint         n_param_values,
                                                     const GValue *param_values,
                                                     gpointer      invocation_hint,
                                                     gpointer      marshal_data);

G_END_DECLS

#endif /* __syl_marshal_MARSHAL_H__ */

