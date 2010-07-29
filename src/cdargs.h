/* File: cdargs.h
 *
 *     This file is part of cdargs
 *
 *     Copyright (C) 2001-2003 by Stefan Kamphausen
 *     Author: Stefan Kamphausen <mail@skamphausen.de>
 *
 *     Time-stamp: <31-Mar-2004 18:15:41 ska>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 


# ifndef CDARGS_H_INCLUDED
# define CDARGS_H_INCLUDED

# define DEBUG 0

// thanks to cscope source for this:
#define CTRL(x)     (x & 037)
#if (BSD || V9) && !__NetBSD__
#define TERMINFO    0   /* no terminfo curses */
#else
#define TERMINFO    1
#endif


/* The main list constructs */
/****************************/

// current list, probably directory listing
vector<pair<string,string> > cur_list;

// the default list which is written from and saved to file
vector<pair<string,string> > default_list;

// an iterator for these
typedef vector<pair<string,string> >::iterator listit;


// Display/Browse Modes;
enum {
   LIST,
   BROWSE
};

// File or Directory?
typedef enum {
   PATH_IS_FILE,
   PATH_IS_DIR
} pathtype;

// needed for map
struct ltstr
{
  bool operator()(const char* s1,const char* s2) const
  {
    return strcmp(s1, s2) < 0;
  }
};


bool   list_from_file(void);
void   list_from_dir(const char* name = ".");
void   list_to_file(void);
bool   do_not_show(const char* name);
void   cur_pos_adjust(int n=0,bool wraparound=true);
bool   entry_nr_exists(unsigned int nr);
string current_entry(void);
void   add_to_default_list(string path, string description="",
                           bool ask_for_desc=false);
void   add_to_list_file(string path);
void   delete_from_default_list(int pos);
void   edit_list_file(void);
//string default_list_file(void);
string get_resultfile(void);
char* get_cwd_as_charp(void);
string get_cwd_as_string(void);
string get_listfile(void);
string capitalized_last_dirname(string path);
string last_dirname(string path);
string canonify_filename(string filename);
bool   valid(string path, pathtype mode);
void   version(void);
void   usage(void);

/* Curses and Display Stuff */
void   init_curses(void);
bool   user_interaction(int c);
void   swap_two_entries(int advance_afterwards);
string get_description_from_user(void);
void   message(const char* msg);
void   display_list(void);
//void scrolling(int lines);
void   update_modeline(void);
void   set_areas(void);
void   resizeevent(int sig);
bool visible(int pos);
int    max_yoffset(void);
void   helpscreen(void);

/* Get outta here */
void   fatal_exit(char* msg);
void   terminate(int sig);
void   finish(string result, bool retval);
void abort_cdargs(void);
# endif
