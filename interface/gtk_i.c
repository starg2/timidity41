/*

    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    gtk_i.c - Glenn Trigg 29 Oct 1998

*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H*/

#include <stdio.h>
#include <string.h>
#ifdef HAVE_GLOB_H
#include <glob.h>
#endif
#include <gtk/gtk.h>

#include "timidity.h"
#include "common.h"
#include "gtk_h.h"

#include "pixmaps/playpaus.xpm"
#include "pixmaps/prevtrk.xpm"
#include "pixmaps/nexttrk.xpm"
#include "pixmaps/rew.xpm"
#include "pixmaps/ff.xpm"
#include "pixmaps/stop.xpm"
#include "pixmaps/quit.xpm"
#include "pixmaps/quiet.xpm"
#include "pixmaps/loud.xpm"
#include "pixmaps/open.xpm"

static GtkWidget *create_menubar(void);
static GtkWidget *create_button_with_pixmap(GtkWidget *, gchar **, gint, gchar *);
static GtkWidget *create_pixmap_label(GtkWidget *, gchar **);
static gint delete_event(GtkWidget *, GdkEvent *, gpointer);
static void destroy (GtkWidget *, gpointer);
static GtkTooltips *create_yellow_tooltips(void);
static void handle_input(gpointer, gint, GdkInputCondition);
static void generic_cb(GtkWidget *, gpointer);
static void generic_scale_cb(GtkAdjustment *, gpointer);
static void open_file_cb(GtkWidget *, gpointer);
static void file_list_cb(GtkWidget *, gint, gint, GdkEventButton *, gpointer);
static gint btn_event_cb(GtkWidget *, GdkEventButton *, gpointer);
static void filer_cb(GtkWidget *, gpointer);
static void tt_toggle_cb(GtkWidget *, gpointer);
static void locate_update_cb(GtkWidget *, GdkEventButton *, gpointer);
static void my_adjustment_set_value(GtkAdjustment *, gint);

static GtkWidget *window, *clist, *text, *vol_scale, *locator;
static GtkWidget *filesel = NULL;
static GtkWidget *tot_lbl, *cnt_lbl, *auto_next, *ttshow;
static GtkTooltips *ttip;
static int file_number_to_play; /* Number of the file we're playing in the list */
static int last_sec, max_sec, is_quitting = 0, locating = 0, local_adjust = 0;

static GtkItemFactoryEntry ife[] = {
    {"/File/Open", "<control>O", open_file_cb, 0, NULL},
    {"/File/Quit", "<control>Q", generic_cb, GTK_QUIT, NULL},
    {"/Options/Auto next", "<control>A", NULL, 0, "<CheckItem>"},
    {"/Options/Show tooltips", "<control>T", tt_toggle_cb, 0, "<CheckItem>"}
};

/*----------------------------------------------------------------------*/

static void
generic_cb(GtkWidget *widget, gpointer data)
{
    gtk_pipe_int_write((int)data);
    if((int)data == GTK_PAUSE) {
	gtk_label_set(GTK_LABEL(cnt_lbl), "Pause");
    }
}

static void
tt_toggle_cb(GtkWidget *widget, gpointer data)
{
    if( GTK_CHECK_MENU_ITEM(ttshow)->active ) {
	gtk_tooltips_enable(ttip);
    }
    else {
	gtk_tooltips_disable(ttip);
    }
}

static void
open_file_cb(GtkWidget *widget, gpointer data)
{
    if( ! filesel ) {
	filesel = gtk_file_selection_new("Open File");
	gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(filesel));

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button),
			   "clicked",
			   GTK_SIGNAL_FUNC (filer_cb), (gpointer)1);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button),
			   "clicked",
			   GTK_SIGNAL_FUNC (filer_cb), (gpointer)0);
    }

    gtk_widget_show(GTK_WIDGET(filesel));
}

static void
filer_cb(GtkWidget *widget, gpointer data)
{
    gchar *filenames[2];
#ifdef HAVE_GLOB_H
    int i;
    gchar *patt;
    glob_t pglob;

    if((int)data == 1) {
	patt = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
	if(glob(patt, GLOB_BRACE|GLOB_NOMAGIC|GLOB_TILDE, NULL, &pglob))
	    return;
	for( i = 0; i < pglob.gl_pathc; i++) {
	    filenames[0] = pglob.gl_pathv[i];
	    filenames[1] = NULL;
	    gtk_clist_append(GTK_CLIST(clist), filenames);
	}
	globfree(&pglob);
    }
#else
    if((int)data == 1) {
	filenames[0] = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
	filenames[1] = NULL;
	gtk_clist_append(GTK_CLIST(clist), filenames);
    }
#endif
    gtk_widget_hide(filesel);
    gtk_clist_columns_autosize(GTK_CLIST(clist));
}

static void
generic_scale_cb(GtkAdjustment *adj, gpointer data)
{
    if(local_adjust)
	return;

    gtk_pipe_int_write((int)data);

    /* This is a bit of a hack as the volume scale (a GtkVScale) seems
       to have it's minimum at the top which is counter-intuitive. */
    if((int)data == GTK_CHANGE_VOLUME) {
	gtk_pipe_int_write(MAX_AMPLIFICATION - adj->value);
    }
    else {
	gtk_pipe_int_write((int)adj->value*100);
    }
}

static void
file_list_cb(GtkWidget *widget, gint row, gint column,
	     GdkEventButton *event, gpointer data)
{
    gchar *fname;

    gtk_clist_get_text(GTK_CLIST(widget), row, column, &fname);
    gtk_pipe_int_write(GTK_PLAY_FILE);
    gtk_pipe_string_write(fname);
    file_number_to_play=row;
}

static gint
btn_event_cb(GtkWidget *widget, GdkEventButton *ev, gpointer data)
{
    fprintf(stderr, "In btn_event_cb.\n");
    return 0;
}

static gint
delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return (FALSE);
}

static void
destroy (GtkWidget *widget, gpointer data)
{
    is_quitting = 1;
    gtk_pipe_int_write(GTK_QUIT);
}

static void
locate_update_cb (GtkWidget *widget, GdkEventButton *ev, gpointer data)
{
    if( (ev->button == 1) || (ev->button == 2)) {
	if( ev->type == GDK_BUTTON_RELEASE ) {
	    locating = 0;
	}
	else {
	    locating = 1;
	}
    }
}

static void
my_adjustment_set_value(GtkAdjustment *adj, gint value)
{
    local_adjust = 1;
    gtk_adjustment_set_value(adj, (gfloat)value);
    local_adjust = 0;
}

void
Launch_Gtk_Process(int pipe_number)
{
    int	argc = 0;
    GtkWidget *button, *mbar, *swin;
    GtkWidget *table, *align, *handlebox;
    GtkWidget *vbox, *hbox, *vscrollbar, *vbox2;
    GtkObject *adj;

    gtk_init (&argc, NULL);

    ttip = create_yellow_tooltips();
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(window, "Timidity - MIDI Player");
    gtk_window_set_title(GTK_WINDOW(window), "Timidity - MIDI Player");

    gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		       GTK_SIGNAL_FUNC (delete_event), NULL);

    gtk_signal_connect(GTK_OBJECT(window), "destroy",
		       GTK_SIGNAL_FUNC (destroy), NULL);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    mbar = create_menubar();
    gtk_box_pack_start(GTK_BOX(vbox), mbar, FALSE, FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 4);
    gtk_widget_show(hbox);

    text = gtk_text_new(NULL, NULL);
    gtk_widget_show(text);
    gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 4);
    vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
    gtk_box_pack_start(GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 4);
    gtk_widget_show (vscrollbar);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);
    gtk_widget_show(hbox);

    adj = gtk_adjustment_new(0., 0., 100., 1., 20., 0.);
    locator = gtk_hscale_new(GTK_ADJUSTMENT(adj));
    gtk_scale_set_draw_value(GTK_SCALE(locator), TRUE);
    gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
			GTK_SIGNAL_FUNC(generic_scale_cb),
			(gpointer)GTK_CHANGE_LOCATOR);
    gtk_signal_connect(GTK_OBJECT(locator), "button_press_event",
			GTK_SIGNAL_FUNC(locate_update_cb),
			NULL);
    gtk_signal_connect(GTK_OBJECT(locator), "button_release_event",
			GTK_SIGNAL_FUNC(locate_update_cb),
			NULL);
    gtk_range_set_update_policy(GTK_RANGE(locator),
				GTK_UPDATE_DISCONTINUOUS);
    gtk_scale_set_digits(GTK_SCALE(locator), 0);
    gtk_widget_show(locator);
    gtk_box_pack_start(GTK_BOX(hbox), locator, TRUE, TRUE, 4);

    align = gtk_alignment_new(0., 1., 1., 0.);
    gtk_widget_show(align);
    cnt_lbl = gtk_label_new("00:00");
    gtk_widget_show(cnt_lbl);
    gtk_container_add(GTK_CONTAINER(align), cnt_lbl);
    gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, TRUE, 0);

    align = gtk_alignment_new(0., 1., 1., 0.);
    gtk_widget_show(align);
    tot_lbl = gtk_label_new("/00:00");
    gtk_widget_show(tot_lbl);
    gtk_container_add(GTK_CONTAINER(align), tot_lbl);
    gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 4);

    swin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    clist = gtk_clist_new(1);
    gtk_container_add(GTK_CONTAINER(swin), clist);
    gtk_widget_show(swin);
    gtk_widget_show(clist);
    gtk_widget_set_usize(clist, 200, 10);
    gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
    gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 1, TRUE);
    gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		       GTK_SIGNAL_FUNC(file_list_cb), NULL);

    gtk_box_pack_start(GTK_BOX(hbox), swin, TRUE, TRUE, 0);

    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);
    gtk_widget_show(vbox2);

    /* This is so the pixmap creation works properly. */
    gtk_widget_realize(window);

    gtk_box_pack_start(GTK_BOX(vbox2),
		       create_pixmap_label(window, loud_xpm),
		       FALSE, FALSE, 0);

    adj = gtk_adjustment_new(30., 0., (gfloat)MAX_AMPLIFICATION, 1., 20., 0.);
    vol_scale = gtk_vscale_new(GTK_ADJUSTMENT(adj));
    gtk_scale_set_draw_value(GTK_SCALE(vol_scale), FALSE);
    gtk_signal_connect (GTK_OBJECT(adj), "value_changed",
			GTK_SIGNAL_FUNC(generic_scale_cb),
			(gpointer)GTK_CHANGE_VOLUME);
    gtk_range_set_update_policy(GTK_RANGE(vol_scale),
				GTK_UPDATE_DELAYED);
    gtk_widget_show(vol_scale);
    gtk_tooltips_set_tip(ttip, vol_scale, "Volume control", NULL);

    gtk_box_pack_start(GTK_BOX(vbox2), vol_scale, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox2),
		       create_pixmap_label(window, quiet_xpm),
		       FALSE, FALSE, 0);

    handlebox = gtk_handle_box_new();
    gtk_box_pack_start(GTK_BOX(hbox), handlebox, FALSE, FALSE, 0);

    table = gtk_table_new(5, 2, TRUE);
    gtk_container_add(GTK_CONTAINER(handlebox), table);

    button = create_button_with_pixmap(window, playpaus_xpm, GTK_PAUSE,
				       "Play/Pause");
    gtk_table_attach_defaults(GTK_TABLE(table), button,
			      0, 2, 0, 1);

    button = create_button_with_pixmap(window, prevtrk_xpm, GTK_PREV,
				       "Previous file");
    gtk_table_attach_defaults(GTK_TABLE(table), button,
			      0, 1, 1, 2);

    button = create_button_with_pixmap(window, nexttrk_xpm, GTK_NEXT,
				       "Next file");
    gtk_table_attach_defaults(GTK_TABLE(table), button,
			      1, 2, 1, 2);

    button = create_button_with_pixmap(window, rew_xpm, GTK_RWD,
				       "Rewind");
    gtk_table_attach_defaults(GTK_TABLE(table), button,
			      0, 1, 2, 3);

    button = create_button_with_pixmap(window, ff_xpm, GTK_FWD,
				       "Fast forward");
    gtk_table_attach_defaults(GTK_TABLE(table), button,
			      1, 2, 2, 3);

    button = create_button_with_pixmap(window, stop_xpm, GTK_RESTART,
				       "Restart");
    gtk_table_attach_defaults(GTK_TABLE(table), button,
			      0, 1, 3, 4);

    button = create_button_with_pixmap(window, open_xpm, 0,
				       "Open");
    gtk_signal_disconnect_by_func(GTK_OBJECT(button), generic_cb, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
			      GTK_SIGNAL_FUNC(open_file_cb), 0);
    gtk_table_attach_defaults(GTK_TABLE(table), button,
			      1, 2, 3, 4);

    button = create_button_with_pixmap(window, quit_xpm, GTK_QUIT,
				       "Quit");
    gtk_table_attach_defaults(GTK_TABLE(table), button,
			      0, 2, 4, 5);

    gtk_widget_show(hbox);
    gtk_widget_show(vbox);
    gtk_widget_show(table);
    gtk_widget_show(handlebox);
    gtk_widget_show(window);

    gdk_input_add(pipe_number, GDK_INPUT_READ, handle_input, NULL);

    gtk_main();
}

static GtkWidget *
create_button_with_pixmap(GtkWidget *window, gchar **bits, gint data, gchar *thelp)
{
    GtkWidget	*pw, *button;
    GdkPixmap	*pixmap;
    GdkBitmap	*mask;
    GtkStyle	*style;

    style = gtk_widget_get_style(window);
    pixmap = gdk_pixmap_create_from_xpm_d(window->window,
					  &mask,
					  &style->bg[GTK_STATE_NORMAL],
					  bits);
    pw = gtk_pixmap_new(pixmap, NULL);
    gtk_widget_show(pw);

    button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button), pw);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
			      GTK_SIGNAL_FUNC(generic_cb),
			      (gpointer)data);
    gtk_widget_show(button);
    gtk_tooltips_set_tip(ttip, button, thelp, NULL);

    return button;
}

static GtkWidget *
create_pixmap_label(GtkWidget *window, gchar **bits)
{
    GtkWidget	*pw;
    GdkPixmap	*pixmap;
    GdkBitmap	*mask;
    GtkStyle	*style;

    style = gtk_widget_get_style(window);
    pixmap = gdk_pixmap_create_from_xpm_d(window->window,
					  &mask,
					  &style->bg[GTK_STATE_NORMAL],
					  bits);
    pw = gtk_pixmap_new(pixmap, NULL);
    gtk_widget_show(pw);

    return pw;
}

static GtkWidget *
create_menubar(void)
{
    GtkItemFactory *ifactory;
    GtkAccelGroup *ag;

    ag = gtk_accel_group_get_default();
    ifactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<Main>", ag);
    gtk_item_factory_create_items(ifactory, 4, ife, NULL);
    gtk_widget_show(ifactory->widget);

    auto_next = gtk_item_factory_get_widget(ifactory, "/Options/Auto next");
    gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(auto_next), TRUE);
    ttshow = gtk_item_factory_get_widget(ifactory, "/Options/Show tooltips");
    gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(ttshow), TRUE);

    return ifactory->widget;
}

/* Following function curtesy of the gtk mailing list. */

static GtkTooltips *
create_yellow_tooltips()
{
    GdkColor *t_fore, *t_back;
    GtkTooltips * tip;

    t_fore = (GdkColor*)g_malloc( sizeof(GdkColor));
    t_back = (GdkColor*)g_malloc( sizeof(GdkColor));

    /* First create a default Tooltip */
    tip = gtk_tooltips_new();

    /* Try to get the colors */
    if ( gdk_color_parse("linen", t_back)){
	if(gdk_color_alloc(gdk_colormap_get_system(), t_back)) {
	    gdk_color_black(gdk_colormap_get_system(), t_fore);
	    gtk_tooltips_set_colors(tip, t_back, t_fore);
	}
    }

    return tip;
}

/* Receive DATA sent by the application on the pipe     */

static void
handle_input(gpointer client_data, gint source, GdkInputCondition ic)
{
    int message;

    gtk_pipe_int_read(&message);

    switch (message) {
    case REFRESH_MESSAGE:
	printf("REFRESH MESSAGE IS OBSOLETE !!!\n");
	break;

    case TOTALTIME_MESSAGE:
	{
	    int cseconds;
	    int minutes,seconds;
	    char local_string[20];
	    GtkObject *adj;

	    gtk_pipe_int_read(&cseconds);

	    seconds=cseconds/100;
	    minutes=seconds/60;
	    seconds-=minutes*60;
	    sprintf(local_string,"/ %i:%02i",minutes,seconds);
	    gtk_label_set(GTK_LABEL(tot_lbl), local_string);

	    /* Readjust the time scale */
	    max_sec=cseconds/100;
	    adj = gtk_adjustment_new(0., 0., (gfloat)max_sec,
				     1., 10., 0.);
	    gtk_signal_connect(GTK_OBJECT(adj), "value_changed",
			       GTK_SIGNAL_FUNC(generic_scale_cb),
			       (gpointer)GTK_CHANGE_LOCATOR);
	    gtk_range_set_adjustment(GTK_RANGE(locator),
				     GTK_ADJUSTMENT(adj));
	}
	break;

    case MASTERVOL_MESSAGE:
	{
	    int volume;
	    GtkAdjustment *adj;

	    gtk_pipe_int_read(&volume);
	    adj = gtk_range_get_adjustment(GTK_RANGE(vol_scale));
	    my_adjustment_set_value(adj, MAX_AMPLIFICATION - volume);
	}
	break;

    case FILENAME_MESSAGE:
	{
	    char filename[255], separator[255], title[255];
	    char *pc;
	    int i;

	    gtk_pipe_string_read(filename);

	    /* Extract basename of the file */
	    pc = strrchr(filename, '/');
	    if (pc == NULL)
		pc = filename;
	    else
		pc++;

	    sprintf(title, "Timidity %s - %s", timidity_version, filename);
	    gtk_window_set_title(GTK_WINDOW(window), title);

	    for (i = 0; i < 30; i++)
		separator[i]='*';
	    separator[i++]='\n';
	    separator[i]='\0';
	    gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
			    separator, -1);
	}
	break;

    case FILE_LIST_MESSAGE:
	{
	    gchar filename[255], *fnames[2];
	    gint i, number_of_files, row;

	    /* reset the playing list : play from the start */
	    file_number_to_play = -1;

	    gtk_pipe_int_read(&number_of_files);
	    for (i = 0; i < number_of_files; i++)
	    {
		gtk_pipe_string_read(filename);
		fnames[0] = filename;
		fnames[1] = NULL;
		row = gtk_clist_append(GTK_CLIST(clist), fnames);
	    }
	    gtk_clist_columns_autosize(GTK_CLIST(clist));
	}
	break;

    case NEXT_FILE_MESSAGE:
    case PREV_FILE_MESSAGE:
    case TUNE_END_MESSAGE:
	{
	    int nbfile;

	    /* When a file ends, launch next if auto_next toggle */
	    if ( (message==TUNE_END_MESSAGE) &&
		 !GTK_CHECK_MENU_ITEM(auto_next)->active )
		return;

	    /* Total number of file to play in the list */
	    nbfile = GTK_CLIST(clist)->rows;

	    if (message == PREV_FILE_MESSAGE)
		file_number_to_play--;
	    else
		file_number_to_play++;

	    /* Do nothing if requested file is before first one */
	    if (file_number_to_play < 0) {
		file_number_to_play = 0;
		return;
	    }

	    /* Stop after playing the last file */
	    if (file_number_to_play >= nbfile) {
		file_number_to_play = nbfile - 1;
		return;
	    }

	    if(gtk_clist_row_is_visible(GTK_CLIST(clist),
					file_number_to_play) !=
	       GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(GTK_CLIST(clist), file_number_to_play,
				 -1, 1.0, 0.0);
	    }
	    gtk_clist_select_row(GTK_CLIST(clist), file_number_to_play, 0);
	}
	break;

    case CURTIME_MESSAGE:
	{
	    int cseconds;
	    int  sec,seconds, minutes;
	    int nbvoice;
	    char local_string[20];

	    gtk_pipe_int_read(&seconds);
	    gtk_pipe_int_read(&nbvoice);

	    if( is_quitting )
		return;

	    sec = seconds;

	    /* To avoid blinking */
	    if (sec!=last_sec)
	    {
		minutes=seconds/60;
		seconds-=minutes*60;

		sprintf(local_string,"%2d:%02d",
			minutes, seconds);

		gtk_label_set(GTK_LABEL(cnt_lbl), local_string);
	    }

	    last_sec=sec;

	    /* Readjust the time scale if not dragging the scale */
	    if( !locating && (cseconds <= max_sec)) {
		GtkAdjustment *adj;

		adj = gtk_range_get_adjustment(GTK_RANGE(locator));
		my_adjustment_set_value(adj, (gfloat)seconds);
	    }
	}
	break;

    case NOTE_MESSAGE:
	{
	    int channel;
	    int note;

	    gtk_pipe_int_read(&channel);
	    gtk_pipe_int_read(&note);
	    printf("NOTE chn%i %i\n",channel,note);
	}
	break;

    case PROGRAM_MESSAGE:
	{
	    int channel;
	    int pgm;

	    gtk_pipe_int_read(&channel);
	    gtk_pipe_int_read(&pgm);
	    printf("NOTE chn%i %i\n",channel,pgm);
	}
	break;

    case VOLUME_MESSAGE:
	{
	    int channel;
	    int volume;

	    gtk_pipe_int_read(&channel);
	    gtk_pipe_int_read(&volume);
	    printf("VOLUME= chn%i %i \n",channel, volume);
	}
	break;


    case EXPRESSION_MESSAGE:
	{
	    int channel;
	    int express;

	    gtk_pipe_int_read(&channel);
	    gtk_pipe_int_read(&express);
	    printf("EXPRESSION= chn%i %i \n",channel, express);
	}
	break;

    case PANNING_MESSAGE:
	{
	    int channel;
	    int pan;

	    gtk_pipe_int_read(&channel);
	    gtk_pipe_int_read(&pan);
	    printf("PANNING= chn%i %i \n",channel, pan);
	}
	break;

    case SUSTAIN_MESSAGE:
	{
	    int channel;
	    int sust;

	    gtk_pipe_int_read(&channel);
	    gtk_pipe_int_read(&sust);
	    printf("SUSTAIN= chn%i %i \n",channel, sust);
	}
	break;

    case PITCH_MESSAGE:
	{
	    int channel;
	    int bend;

	    gtk_pipe_int_read(&channel);
	    gtk_pipe_int_read(&bend);
	    printf("PITCH BEND= chn%i %i \n",channel, bend);
	}
	break;

    case RESET_MESSAGE:
	printf("RESET_MESSAGE\n");
	break;

    case CLOSE_MESSAGE:
	gtk_exit(0);
	break;

    case CMSG_MESSAGE:
	{
	    int type;
	    char message[1000];

	    gtk_pipe_int_read(&type);
	    gtk_pipe_string_read(message);
	    gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
			    message, -1);
	    gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
			    "\n", 1);
	}
	break;
    default:
	fprintf(stderr,"UNKNOWN Gtk+ MESSAGE %i\n",message);
    }
}
