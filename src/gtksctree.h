/* Mail Summary tree widget for Sylpheed */

#ifndef __GTK_SCTREE_H__
#define __GTK_SCTREE_H__

#include <gtk/gtk.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkctree.h>

/* This code is based on "gtkflist.{h,c}" from mc-4.5.42 .*/

#define TYPE_GTK_SCTREE			(gtk_sctree_get_type ())
#define GTK_SCTREE(obj)			(GTK_CHECK_CAST ((obj), TYPE_GTK_SCTREE, GtkSCTree))
#define GTK_SCTREE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), TYPE_GTK_SCTREE, GtkSCTreeClass))
#define GTK_IS_SCTREE(obj)		(GTK_CHECK_TYPE ((obj), TYPE_GTK_SCTREE))
#define GTK_IS_SCTREE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), TYPE_GTK_SCTREE))


typedef struct _GtkSCTree GtkSCTree;
typedef struct _GtkSCTreeClass GtkSCTreeClass;

struct _GtkSCTree {
	GtkCTree ctree;

	/* The anchor row for range selections */
	GtkCTreeNode *anchor_row;

	/* Mouse button and position saved on button press */
	gint dnd_press_button;
	gint dnd_press_x, dnd_press_y;

	/* Delayed selection information */
	gint dnd_select_pending;
	guint dnd_select_pending_state;
	gint dnd_select_pending_row;
};

struct _GtkSCTreeClass {
    	GtkCTreeClass parent_class;
    
    	/* Signal: invoke the popup menu for rows */
    	void (* row_popup_menu) (GtkSCTree *sctree, GdkEventButton *event);
    
    	/* Signal: invoke the popup menu for empty areas */
    	void (* empty_popup_menu) (GtkSCTree *sctree, GdkEventButton *event);

	/* Signal: open the file in the selected row */
	void (* open_row) (GtkSCTree *sctree);

    	/* Signal: initiate a drag and drop operation */
    	void (* start_drag) (GtkSCTree *sctree, gint button, GdkEvent *event);
};


GType gtk_sctree_get_type (void);
GtkWidget *gtk_sctree_new_with_titles	(gint		 columns, 
					 gint		 tree_column, 
					 gchar		*titles[]);
void gtk_sctree_select			(GtkSCTree	*sctree,
					 GtkCTreeNode	*node);
void gtk_sctree_unselect_all		(GtkSCTree	*sctree);

void gtk_sctree_set_anchor_row		(GtkSCTree	*sctree,
					 GtkCTreeNode	*node);

#endif /* __GTK_SCTREE_H__ */
