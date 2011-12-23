/**
 * progress.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * progress bar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include "config.h"
#include "progress.h"
#include "gettext.h"
#define _(STRING) gettext(STRING)
#include "partclone.h"

#ifdef HAVE_LIBNCURSESW
#include <ncurses.h>
extern WINDOW *p_win;
extern WINDOW *bar_win;
int color_support = 1;
#endif

int PUI;
unsigned long RES=0;

/// initial progress bar
extern void progress_init(struct progress_bar *prog, int start, unsigned long long stop, int size)
{
    memset(prog, 0, sizeof(progress_bar));
    prog->start = start;
    prog->stop = stop;
    prog->unit = 100.0 / (stop - start);
    prog->initial_time = time(0);
    prog->resolution_time = time(0);
    prog->interval_time = 1;
    prog->block_size = size;
    if (RES){
        prog->interval_time = RES;
    }
    prog->rate = 0.0;
    prog->pui = PUI;
}

/// open progress interface
extern int open_pui(int pui, unsigned long res){
    int tui = 0;
    if (pui == NCURSES){
        tui = open_ncurses();
        if (tui == 0){
            close_ncurses();
        }
    } else if (pui == DIALOG){
        tui = 1;
    }
    PUI = pui;
    RES = res;
    return tui;
}

/// close progress interface
extern void close_pui(int pui){
    if (pui == NCURSES){
        close_ncurses();
    }
}

extern void update_pui(struct progress_bar *prog, unsigned long long current, int done){
    if (prog->pui == NCURSES)
        Ncurses_progress_update(prog, current, done);
    else if (prog->pui == DIALOG)
        Dialog_progress_update(prog, current, done);
    else if (prog->pui == TEXT)
        progress_update(prog, current, done);
}

static void calculate_speed(struct progress_bar *prog, unsigned long long current, int done, prog_stat_t *prog_stat){
    char *format = "%H:%M:%S";
    uint64_t speedps = 1;
    uint64_t speed = 1;
    double dspeed = 1.0;
    float percent = 1.0;
    time_t remained;
    time_t elapsed;
    char Rformated[10], Eformated[10];
    char speed_unit[] = "    ";
    struct tm *Rtm, *Etm;
    uint64_t gbyte=1000000000.0;
    uint64_t mbyte=1000000;
    uint64_t kbyte=1000;


    percent  = prog->unit * current;
    if (percent <= 0)
	percent = 1;
    else if (percent >= 100)
	percent = 99.99;


    elapsed  = (time(0) - prog->initial_time);
    if (elapsed <= 0)
	elapsed = 1;

    speedps  = prog->block_size * current / elapsed;
    speed = speedps * 60.0;

    if(prog->block_size == 1){ // don't show bitmap rate, bit_size = 1
	speed = 0;	
    }

    prog_stat->percent   = percent;

    if (speed >= gbyte){
	dspeed = (double)speed / (double)gbyte;
	strncpy(speed_unit, "GB", 3);
	strncpy(prog_stat->speed_unit, speed_unit, 3);
    }else if (speed >= mbyte){
	dspeed = (double)speed / (double)mbyte;
	strncpy(speed_unit, "MB", 3);
	strncpy(prog_stat->speed_unit, speed_unit, 3);
    }else if (speed >= kbyte){
	dspeed = (double)speed / (double)kbyte;
	strncpy(speed_unit, "KB", 3);
	strncpy(prog_stat->speed_unit, speed_unit, 3);
    }else{
	dspeed = speed;
	strncpy(speed_unit, "byte", 5);
	strncpy(prog_stat->speed_unit, speed_unit, 5);
    }

    prog_stat->speed     = dspeed;

    if (done != 1){
        remained = (time_t)((elapsed/percent*100) - elapsed);

        Rtm = gmtime(&remained);
        strftime(Rformated, sizeof(Rformated), format, Rtm);

        Etm = gmtime(&elapsed);
        strftime(Eformated, sizeof(Eformated), format, Etm);

    } else {
        prog_stat->percent=100;
        remained = (time_t)0;
        Rtm = gmtime(&remained);
        strftime(Rformated, sizeof(Rformated), format, Rtm);
        Etm = gmtime(&elapsed);
        strftime(Eformated, sizeof(Eformated), format, Etm);
    }

    strncpy(prog_stat->Eformated, Eformated, 10);
    strncpy(prog_stat->Rformated, Rformated, 10);
}

/// update information at progress bar
extern void progress_update(struct progress_bar *prog, unsigned long long current, int done)
{
    char clear_buf = ' ';
    prog_stat_t prog_stat;

    if (done != 1)
	if ((difftime(time(0), prog->resolution_time) < prog->interval_time) && current != 0)
	    return;

    memset(&prog_stat, 0, sizeof(prog_stat_t));
    calculate_speed(prog, current, done, &prog_stat);

    if (done != 1){
	prog->resolution_time = time(0);
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%"), clear_buf, prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent);

	if((int)prog_stat.speed > 0)
	    fprintf(stderr, _(", %6.2f%s/min,"), prog_stat.speed, prog_stat.speed_unit);
	//fprintf(stderr, "current block=%lld%s", current, "\x1b[A");
    } else {
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed: 100.00%%"), clear_buf, prog_stat.Eformated, prog_stat.Rformated);
	if((int)prog_stat.speed > 0)
	   fprintf(stderr, _(", Rate: %6.2f%s/min,"), prog_stat.speed, prog_stat.speed_unit);

        fprintf(stderr, _("\nTotal Time: %s, "), prog_stat.Eformated);
	if((int)prog_stat.speed > 0)
	    fprintf(stderr, _("Ave. Rate: %6.1f%s/min, "), prog_stat.speed, prog_stat.speed_unit);
        fprintf(stderr, _("%s"), "100.00% completed!\n");
    }
}

/// update information at ncurses mode
extern void Ncurses_progress_update(struct progress_bar *prog, unsigned long long current, int done)
{
#ifdef HAVE_LIBNCURSESW

    char *p_block;
    prog_stat_t prog_stat;

    memset(&prog_stat, 0, sizeof(prog_stat_t));
    calculate_speed(prog, current, done, &prog_stat);

    /// set bar color
    init_pair(4, COLOR_RED, COLOR_RED);
    init_pair(5, COLOR_WHITE, COLOR_BLUE);
    init_pair(6, COLOR_WHITE, COLOR_RED);
    werase(p_win);
    werase(bar_win);

    if (done != 1){
        if (difftime(time(0), prog->resolution_time) < prog->interval_time)
            return;
	prog->resolution_time = time(0);

        mvwprintw(p_win, 0, 0, _(" "));
        mvwprintw(p_win, 1, 0, _("Elapsed: %s") , prog_stat.Eformated);
        mvwprintw(p_win, 2, 0, _("Remaining: %s"), prog_stat.Rformated);
	if ((int)prog_stat.speed > 0)
	    mvwprintw(p_win, 3, 0, _("Rate: %6.2f%s/min"), prog_stat.speed, prog_stat.speed_unit);
        p_block = calloc(sizeof(char), 50);
        memset(p_block, ' ', (size_t)(prog_stat.percent*0.5));
        wattrset(bar_win, COLOR_PAIR(4));
        mvwprintw(bar_win, 0, 0, "%s", p_block);
        wattroff(bar_win, COLOR_PAIR(4));
	free(p_block);
        mvwprintw(p_win, 5, 52, "%6.2f%%", prog_stat.percent);
        wrefresh(p_win);
        wrefresh(bar_win);
    } else {
        mvwprintw(p_win, 1, 0, _("Total Time: %s"), prog_stat.Eformated);
        mvwprintw(p_win, 2, 0, _("Remaining: 0"));
	if ((int)prog_stat.speed > 0)
	    mvwprintw(p_win, 3, 0, _("Ave. Rate: %6.2f%s/min"), prog_stat.speed, prog_stat.speed_unit);

        wattrset(bar_win, COLOR_PAIR(4));
	mvwprintw(bar_win, 0, 0, "%50s", " ");
        wattroff(bar_win, COLOR_PAIR(4));

        mvwprintw(p_win, 5, 52, "100.00%%");
        wrefresh(p_win);
        wrefresh(bar_win);
        refresh();
	sleep(1);
    }

#endif
}

/// update information as dialog format, refernece source code of dialog
/// # mkfifo pipe
/// # (./clone.extfs -d -c -X -s /dev/loop0 2>pipe | cat - > test.img) | ./gauge < pipe
/// # (cat test - |./clone.extfs -d -X -r -s - -o /dev/loop0 2>pipe) | ./gauge < pipe
extern void Dialog_progress_update(struct progress_bar *prog, unsigned long long current, int done){
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    extern p_dialog_mesg m_dialog;

    char tmp_str[128];
    char *clear_buf = NULL;
    prog_stat_t prog_stat;

    memset(&prog_stat, 0, sizeof(prog_stat_t));
    calculate_speed(prog, current, done, &prog_stat);

    if (done != 1){
        if (difftime(time(0), prog->resolution_time) < prog->interval_time)
            return;
	prog->resolution_time = time(0);

        m_dialog.percent = (int)prog_stat.percent;
	if ((int)prog_stat.speed > 0)
	    sprintf(tmp_str, _("  Elapsed: %s\n  Remaining: %s\n  Completed:%6.2f%%\n  Rate: %6.2f%s/min, "), prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent, (float)(prog_stat.speed), prog_stat.speed_unit);
	else
	    sprintf(tmp_str, _("  Elapsed: %s\n  Remaining: %s\n  Completed:%6.2f%%"), prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent);
        fprintf(stderr, "XXX\n%i\n%s\n%s\nXXX\n", m_dialog.percent, m_dialog.data, tmp_str);
    } else {
        m_dialog.percent = 100;
	if ((int)prog_stat.speed > 0)
	    sprintf(tmp_str, _("  Total Time: %s\n  Ave. Rate: %6.1f%s/min\n 100.00%%  completed!\n"), prog_stat.Eformated, (float)prog_stat.speed, prog_stat.speed_unit);
	else
	    sprintf(tmp_str, _("  Total Time: %s\n  100.00%%  completed!\n"), prog_stat.Eformated);
        fprintf(stderr, "XXX\n%i\n%s\n%s\nXXX\n", m_dialog.percent, m_dialog.data, tmp_str);
    }

}
