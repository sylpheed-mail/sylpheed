#ifndef __INTL_H__
#define __INTL_H__

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext(String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop(String)
#  else
#    define N_(String) (String)
#  endif /* gettext_noop */
#else
#  define _(String) (String)
#  define N_(String) (String)
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,String) (String)
#  define dcgettext(Domain,String,Type) (String)
#  define bindtextdomain(Domain,Directory) (Domain) 
#endif /* ENABLE_NLS */

#endif /* __INTL_H__ */
