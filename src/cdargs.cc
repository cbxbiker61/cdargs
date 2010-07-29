/* File: cdargs.cc
 *
 *     This file is part of cdargs
 *
 *     Copyright (C) 2001-2003 by Stefan Kamphausen
 *     Author: Stefan Kamphausen <http://www.skamphausen.de>
 *
 *     Time-stamp: <26-Feb-2006 18:06:47 ska>
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

/********************************************************************/
/* MAN, IF YOU DIDN'T TRY TO FIX THIS CODE YOU DON'T KNWO HOW MUCH  */
/*                    IT NEEDS A COMPLETE REWRITE                   */
/*                           (the author)                           */
/********************************************************************/


// damn, if you remove this, you get lots of trouble that _I_ don't
// wanna get involved in. I've been reading include files for more
// than one hour now, and that's my decision.  The code is a mess
// anyway. 
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

/* C++ Includes */
# include <iostream>
# include <string>
# include <vector>
# include <algorithm>
# include <fstream>
# include <map>
using namespace std;

/* C Includes */
# include <stdio.h>
# include <stdlib.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <dirent.h>
# include <getopt.h>
# include <unistd.h>
# include <signal.h>
# include <string.h>

//# if defined(USE_NCURSES) && !defined(RENAMED_NCURSES)
# if defined(HAVE_NCURSES_H)
# include <ncurses.h>
# else
# include <curses.h>
# endif

# include "cdargs.h"

/* GLOBALS */
// FIXME: this file has too many globals. Especially the
// offset/position handling is UGLY!
//

// Change this if you want different length (actually width) for the
// description:
# define DESCRIPTION_MAXLENGTH 18
# define DESCRIPTION_BROWSELENGTH 8

const char* DefaultListfile = ".cdargs";
const char* DefaultResultfile = "~/.cdargsresult";

const char* Needle = NULL;
bool NeedleGiven = false;

int CurrPosition = 0;
map<const char*, int, ltstr> LastPositions;

// terminal coordinates and other curses stuff
int  terminal_width, terminal_height;
int  xmax;
int  display_area_ymax;
int  display_area_ymin;
int  modeliney;
int  msg_area_y;
int  yoffset = 0;
bool curses_running = false;
int shorties_offset = 0;

// cdargs modi
int  mode = LIST;
bool show_hidden_files = false;
bool opt_no_wrap = false;
bool opt_no_resolve = false;
bool listfile_empty = false;
bool opt_cwd = false;

string opt_resultfile="";
string opt_listfile="";
string opt_user="";

int main(int argc, char** argv) {
    int c;
    /* parse command line args */
    while (1) {
        int option_index = 0;
        static struct option long_options[] =
            {
                {"add"        , 1, 0, 0},
                {"file"       , 1, 0, 0},
                {"user"       , 1, 0, 0},
                {"browse"     , 0, 0, 0},
                {"nowrap"     , 0, 0, 0},
                {"noresolve"  , 0, 0, 0},
                {"cwd"        , 0, 0, 0},
                {"output"     , 1, 0, 0},
                {"version"    , 0, 0, 0},
                {"help"       , 0, 0, 0},
                {0, 0, 0, 0}
            };
        c = getopt_long (argc, argv, "a:f:u:brco:vh",
                         long_options, &option_index);
        if (c == -1) {
            break;
        }
        string optname;
        string argument;
        switch (c)
            {
            case 0:
                optname = string(long_options[option_index].name);
                if (optname == "help") {
                    version();
                    usage();
                    exit(0);
                }
                if (optname == "version") {
                    version();
                    exit(0);
                }
                if (optname == "add") {
                    argument = string(optarg);
                    add_to_list_file(argument);
                    exit(0);
                }
                if (optname == "file") {
                    opt_listfile = string(optarg);
                }
                if (optname == "user") {
                    opt_user = string(optarg);
                }
                if(optname == "browse") {
                    mode = BROWSE;
                }
                if (optname == "nowrap") {
                    opt_no_wrap = true;
                }
                if (optname == "noresolve") {
                    opt_no_resolve = true;
                }
                if (optname == "cwd") {
                    opt_cwd = true;
                }
                if (optname == "output") {
                    opt_resultfile = string(optarg);
                }
                break;             
            case 'a':
                argument = string(optarg);
                add_to_list_file(argument);
                exit(0);
                break;
            case 'f':
                opt_listfile = string(optarg);
                break;
            case 'u':
                opt_user = string(optarg);
                break;
            case 'b':
                mode = BROWSE;
                break;
            case 'r':
                opt_no_resolve = true;
                break;
            case 'c':
                opt_cwd = true;
                break;
            case 'o':
                opt_resultfile = string(optarg);
                break;
            case 'h':
                version();
                usage();
                exit(0);
                break;
            case 'v':
                version();
                exit(0);
            default:
                usage();
                exit(1);
            }
    }

    if (optind < argc) {
        Needle = argv[optind];
        if(strlen(Needle) > 0) {
            NeedleGiven = true;
        } 
        if(strlen(Needle) == 1 && isdigit(Needle[0])) {
            CurrPosition = atoi(Needle);
            Needle = NULL;
        }
    }
    // leave terminal tidy
    // FIXME: what other signals do I need to catch?
    signal(SIGINT, terminate);
    signal(SIGTERM, terminate);
    signal(SIGSEGV, terminate);
    init_curses();
    // answer to terminal reizing
    signal(SIGWINCH, resizeevent);
    /* get list from file or start in browse mode */
    if (!list_from_file()) {
        /* doesn't exist. browse current dir */
        mode = BROWSE;
    }
    // if we're browsing read the CWD
    if(mode == BROWSE) {
        list_from_dir();
    }

    /* main event loop */
    /* determines the entry to use */
    while (1) {
        display_list();
        c = getch();
        if (!user_interaction(c)) {
            break;
        }
    }
   
    finish(current_entry(), true);
    exit(1);
}

void toggle_mode(void) {
    if (mode == LIST) {
        mode = BROWSE;
        list_from_dir(".");
    } else {
        if(listfile_empty) { // list was empty at start
            if(!default_list.empty()) { // but isn't now 
                list_to_file();
                list_from_file();
                mode = LIST;
            } else {                   // ok, we have nothing to show here
                message("No List entry. Staying in BROWSE mode");
                mode = BROWSE;
                list_from_dir(".");
            }
        } else {         // the "normal" case;
            // disable Needle and reload the list!
            NeedleGiven = false;
            Needle = NULL;
            list_from_file();
            yoffset = 0;
            shorties_offset = 0;
            CurrPosition = 0;
            mode = LIST;
        }
    }
}

void toggle_hidden(void) {
    show_hidden_files = !show_hidden_files;
    if (mode == BROWSE) {
        list_from_dir();
    }
}

bool user_interaction(int c) {
    int num;
    string curen;
    switch(c) {
        // ==== Exits
    case CTRL('['): // vi
    case CTRL('g'): // emacs
        abort_cdargs();
        break;
    case 'q':
        finish(".", false);
        break;
    case 13: // ENTER
        // choose dir at cur pos
        return false;
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        num =  c-'0'+shorties_offset;
        if(mode == LIST) {
            if (entry_nr_exists(num)) {
                CurrPosition = num+yoffset;
                return opt_no_resolve?true:false;
            }
        } else {
            CurrPosition = num+yoffset;
            return true;
        }
        break;      

        // ==== Modes
    case '.':
        // show hidden files
        toggle_hidden();
        break;
    case '\t': // TAB
        toggle_mode();
        break;      

        // ==== Navigate The List
    case 'j':       // vi
    case CTRL('n'): // emacs
#if TERMINFO
    case KEY_DOWN:
#endif
        // navigate list++
        cur_pos_adjust(+1);
        break;
    case 'k':       // vi
    case CTRL('p'): // emacs
#if TERMINFO
    case KEY_UP:
#endif
        // navigate list--
        cur_pos_adjust(-1);
        break;

    case '^':       // vi?
    case CTRL('a'): // emacs
#if TERMINFO
    case KEY_HOME:
#endif
        // go to top
        CurrPosition = 0;
        yoffset = 0;
        break;

    case '$':       // vi
    case CTRL('e'): // emacs
# if TERMINFO
    case KEY_END:
#endif
        // go to end
        if (mode == BROWSE) {
            CurrPosition = cur_list.size()-1;
        }
        else {
            CurrPosition = default_list.size()-1;
        }
        yoffset = max_yoffset();
        break;
      
        //==== move the shortcut digits ('shorties')
        // FIXME case ??:  //vi
        // FIXME maybe change the scrolling behaviour to not take the window but the whole
        // list? That means recentering when shorties leave the screen (adjust yoffset and
        // CurrPosition) 
    case CTRL('v'): // emacs
#if TERMINFO      
    case KEY_NPAGE:
#endif
        for(int i=0;i<10;i++) {
            cur_pos_adjust(+1,false);
        }
        break;

        // fixme: vi?
        //case CTRL(''): // FIXME: META(x)??
#if TERMINFO
    case KEY_PPAGE:
#endif
        for(int i=0;i<10;i++) {
            cur_pos_adjust(-1,false);
        }
        break;      

    case 'h':       // vi
    case CTRL('b'): // emacs
#if TERMINFO
    case KEY_LEFT:
#endif
        // up dir
        if (mode == BROWSE) {
//             LastPositions[get_current_dir_name()] = CurrPosition;
            LastPositions[get_cwd_as_charp()] = CurrPosition;
        } else {
            mode = BROWSE;
        }
        list_from_dir("..");
        break;
    case 'l':       // vi
    case CTRL('f'): // emacs
#if TERMINFO
    case KEY_RIGHT:
#endif
        // descend dir at cur pos
        curen = current_entry();
        if (mode == BROWSE) {
            LastPositions[get_cwd_as_charp()] = CurrPosition;
        } else {
            mode = BROWSE;
        }
        list_from_dir(curen.c_str());
        break;
    case 'H':
    case '?':
        helpscreen();
        break;

    case 'd':
    case CTRL('d'): // emacs
#if TERMINFO
    case KEY_BACKSPACE:
#endif
        // delete dir acp
        if (mode == LIST && !NeedleGiven) {
            delete_from_default_list(CurrPosition);
        } else {
            beep();
        }
        break;

#if TERMINFO
    case KEY_IC:
#endif
    case 'a':
        // add dir acp (if in browse mode)
        if(!NeedleGiven) {
            if (mode == BROWSE) {
                add_to_default_list(current_entry());
            } else {
                add_to_default_list(get_cwd_as_string());
            }
        } else {
            beep();
        }
        break;
    case 'A':
        if(!NeedleGiven) {
            // add dir acp (if in browse mode) (ask for desc)
            if (mode == BROWSE) {
                add_to_default_list(current_entry(),"",true);
            } else {
                add_to_default_list(get_cwd_as_string(),"",true);
            }
        } else {
            beep();
        }
        break;
    case 'c':
        if(!NeedleGiven) {
            // add the current dir in every mode
            add_to_default_list(get_cwd_as_string());
        } else {
            beep();
        }
        break;
    case 'C':
        if(!NeedleGiven) {
            // add the current dir in every mode (ask for desc)
            add_to_default_list(get_cwd_as_string(),"",true);
        } else {
            beep();
        }
        break;

        // ==== EDIT the list
    case 'v':
    case 'e':
    {
        if(!NeedleGiven) {
            edit_list_file();
        } else {
            beep();
        }
        break;
    }
    case 'm':
        swap_two_entries(1);
        break;
    case 'M':
        swap_two_entries(-1);
        break;
    case 't':
    case 's':
        swap_two_entries(0);
        break;

        // ==== Filesystem Hotspots
    case '~':
        mode = BROWSE;
        list_from_dir(getenv("HOME"));
        break;
    case '/':
        mode = BROWSE;
        list_from_dir("/");
        break;
    default:
        beep();
        message("unknown command");
    }
    return true;
}

void
swap_two_entries(int advance_afterwards) {
    int switch_index;
    if(advance_afterwards >= 0) {
        switch_index = CurrPosition + 1;
    } else {
        switch_index = CurrPosition -1;
    }
    // ranges check:
    if(switch_index >= int(default_list.size()) || switch_index < 0) {
        beep();
        return;
    }
    if (mode == LIST && !NeedleGiven) {
        pair<string, string> tmp = default_list[CurrPosition];
        default_list[CurrPosition] = default_list[switch_index];
        default_list[switch_index] = tmp;
        cur_pos_adjust(advance_afterwards);
        display_list();
    } else {
        beep();
    }
}

string get_description_from_user(void) {
    char desc[DESCRIPTION_MAXLENGTH+1];
    move(msg_area_y, 0);
    clrtoeol();
    printw("Enter description (<ENTER> to quit): ");
    refresh();
    echo();
    getnstr(desc, DESCRIPTION_MAXLENGTH);
    noecho();
    move(msg_area_y, 0);
    clrtoeol();

    return string(desc);
}

bool
list_from_file(void) {
   
    string desc;
    string path;
    int linecount=0;
    default_list.clear();
    string mylistfile = get_listfile();
    ifstream listfile(mylistfile.c_str());
    if (!listfile) {
        return false;
    }
    string cwd = get_cwd_as_string();
    while (! listfile.eof()) {
        string line;
        getline(listfile, line);
        if(line == "") continue;
        // comments ... are not saved later so we simply don't allow
        // them
//       if (line[0] == '#') {
//          continue;
//       }
        // detect path and description at the leading slash of the
        // absolute path:
        desc = line.substr(0,line.find('/')-1);
        path = line.substr(line.find('/'));
        if(opt_cwd && cwd == path) {
            CurrPosition = linecount;
        }
        // counting the lines: if only one, no resolving should take place
        linecount++;

        // if we got an exact match and  not opt_no_resolve the first entry is
        // the result!
        if(NeedleGiven && Needle && Needle == desc && !opt_no_resolve) {
            finish(path,true);
        }
        // filtered?
        if (do_not_show(desc.c_str()) && do_not_show(path.c_str())) {
            continue;
        }
        default_list.push_back(make_pair(desc, path));
    }
    listfile.close();
    // some magic:
    switch(linecount) {
    case 0:
        listfile_empty = true;
        break;
    case 1:
        opt_no_resolve=true;
        break;
    }
    shorties_offset = 0;
    if(default_list.empty()) {
        return false;
    }
    return true;
}

// default: name="."
void
list_from_dir(const char* name) {

    string previous_dir = "";
    if(name == "..") {
        previous_dir = get_cwd_as_string();
    }
    // Checking, Changing and Reading
    if (chdir(name) < 0) {
        string msg = "couldn't change to dir: ";
        msg += name;
        message(msg.c_str());
    }
   
    DIR* THISDIR;
    string cwd = get_cwd_as_string();
    THISDIR = opendir(cwd.c_str());
   
    if (THISDIR == NULL) {
        string msg = "couldn't read dir: ";
        msg += cwd;
        message(msg.c_str());
        sleep(1);
        abort_cdargs();
    }
    yoffset = 0;
    shorties_offset = 1;
    struct dirent* en;
    struct stat buf;

    // cleanup
    cur_list.clear();

    CurrPosition = 1;

    // put the current directory on top ef every listing
    string desc = ".";
    cur_list.push_back(make_pair(desc,cwd));

    // read home dir
    while ((en = readdir(THISDIR)) != NULL) {
        /* filter dirs */
        if (do_not_show(en->d_name)) continue;
        string fullname;
        fullname = cwd + string("/") + string(en->d_name);

        fullname=canonify_filename(fullname);
      
        stat(fullname.c_str(), &buf);
        if (!S_ISDIR(buf.st_mode)) continue;
      
        string lastdir = last_dirname(fullname);
        cur_list.push_back(make_pair(lastdir, fullname));
    }
    // empty directory listing: .. and .
    desc="..";
    string path=get_cwd_as_string();
    if(cur_list.size() == 1) {
        path = path.substr(0,path.find(last_dirname(path)));
        cur_list.push_back(make_pair(desc,path));
      
    }
    sort(cur_list.begin(),cur_list.end());

    // have we remembered a current position for this dir?
    if (LastPositions.count(cwd.c_str())) {
        CurrPosition = LastPositions[cwd.c_str()];
    } else {
        //CurrPosition = 1;
        if(previous_dir.length() > 0) {
            int count = 0;
            for(listit it=cur_list.begin();
                it!=cur_list.end();++it, count++) {
                if(it->second == previous_dir) {
                    CurrPosition = count;
                    break;
                }
            }
        }
    }
    if(!visible(CurrPosition)) {
        yoffset = CurrPosition - 1;
    }

    closedir(THISDIR);
}

void list_to_file(void) {
    // don't write empty files FIXME: unlink file?
    string mylistfile = get_listfile();
    if(default_list.empty()) {
        unlink(mylistfile.c_str());
        return;
    }
    // never touch the listfile of another user
    if(opt_user.size() > 0) {
        return;
    }
    string backup_file = mylistfile + "~";
    if(rename(mylistfile.c_str(),backup_file.c_str()) == -1) {
        fprintf(stderr,"warning: could not create backupfile\n");
    }
    ofstream listfile(mylistfile.c_str());
    if (!listfile) {
        fatal_exit("couldn't write listfile");
    }
    for (listit li = default_list.begin();li != default_list.end(); ++li) {
        listfile << li->first << " " << li->second << endl;
    }
    listfile.close();
}

bool do_not_show(const char* name) {
    /* don't show current, up, hidden if not flag */
    if (mode == BROWSE) {
        if (strcmp(name, "..") == 0)
            return true;
        if (!show_hidden_files && name[0] == '.')
            return true;
    } else {
        if (!NeedleGiven) {
            return false;
        }
        if(!Needle)  return false;
        /* FIXME case-insensitive comparison */
        //if (strcasecmp(name, Needle)) {
        if (strstr(name, Needle)) {
            return false;
        } else {
            return true;
        }
    }
    return false;
}

// default: n=0,
//          wraparound=true
void cur_pos_adjust(int n, bool wraparound) {
    int newpos = CurrPosition + n;
    int max, min;
   
    if (mode == LIST) {
        max = default_list.size() - 1;
    } else {
        max = cur_list.size() - 1;
    }
    min = 0;   
    if (newpos < min) {
        if (opt_no_wrap || !wraparound) return;
        newpos = max;
        yoffset = max_yoffset();
    }
    if (newpos > max) {
        if (opt_no_wrap || !wraparound) return;
        newpos = min;
        yoffset = 0;
    }
    CurrPosition = newpos;
    // scrolling...
    if (CurrPosition-yoffset >= display_area_ymax) {
        yoffset++;
    }
    if (yoffset > 0 && CurrPosition == yoffset) {
        yoffset--;
    }

}

bool entry_nr_exists(unsigned int nr) {
    if (mode == LIST) {
        return nr<default_list.size() ? true:false;
    }
    if (mode == BROWSE) {
        return nr<cur_list.size() ? true:false;
    }
    return false;
}

string current_entry(void) {
    string res;
    if (mode == LIST) {
        if (default_list.empty()) {
            return string(".");
        }
        res = default_list[CurrPosition].second;
    }
    if (mode == BROWSE) {
        if (cur_list.empty()) {
            return string(".");
        }
        res = cur_list[CurrPosition].second;
    }
    return (res == "") ? string("."):res;
}

// default: description=""
// default: ask_for_desc=false
void add_to_default_list(string path,
                         string description,
                         bool ask_for_desc) {

    //
    if (cur_list.empty() && curses_running) {
        path = get_cwd_as_string();
    }

    // get the description (either from user or generic)
    string desc;
    if(description.size() == 0) {
        if(ask_for_desc && curses_running) {
            desc = get_description_from_user();
            if(desc.size()==0) { // empty string is quit
                return;
            }
        } else {
            desc = last_dirname(path);
        }
    } else {
        desc = description;
    }
   
    string msg = "added :" +desc+":" + path;
    default_list.push_back(make_pair(desc, path));
    message(msg.c_str());
}

void add_to_list_file(string path) {
    // get rid of leading = if there
    if (path.at(0) == '=') {
        path = path.substr(1);
    }   
    // the syntax for passing descriptions from the command line is:
    // --add=:desc:/absolute/path
    string desc;
    if(path.at(0) == ':') {
        int colon2_at;
        colon2_at = path.find(":",1);
        if(colon2_at>(DESCRIPTION_MAXLENGTH+1)) {
            fprintf(stderr,"description too long! max %d chars\n", DESCRIPTION_MAXLENGTH);
            exit(-4);
        } else {
            desc = path.substr(1,colon2_at-1);
        }
        path = path.substr(colon2_at+1);
    }
    // FIXME: check for existance here?
    if(path.at(0) != '/') {
        fprintf(stderr,"this is not an absolute path:\n%s\n",path.c_str());
        exit(-2);
    }
    list_from_file();
    add_to_default_list(path,desc,false);
    list_to_file();
}

void delete_from_default_list(int pos) {
    int count = 0;
    for (listit li = default_list.begin();li != default_list.end(); ++li) {
        if (count == pos) {
            default_list.erase(li);
            cur_pos_adjust(0,false);
            // we want no one-entry-in-list-magic anymore
            opt_no_resolve=true;
            return;
        } else {
            count++;
        }
    }
}

void edit_list_file(void) {
    char* editor = getenv("EDITOR");
    if (!editor) {
        // FIXME: how to detect debian and the /usr/bin/editor rule?
        struct stat buf;
        if(stat("/usr/bin/editor",&buf) == 0)
            editor = "/usr/bin/editor";
        else
            editor = "vi";
    }
    endwin();
    list_to_file();
    system((string(editor) + " \"" + get_listfile() + "\"").c_str());
    list_from_file();
    refresh();
    init_curses();
    display_list();
}

string get_resultfile(void) {
    if(opt_resultfile.size() >0) {
        return opt_resultfile;
    }
    return string(DefaultResultfile);
}

char* get_cwd_as_charp(void) {
    char buf[PATH_MAX];
    char* result = getcwd(buf, sizeof(buf));

    if(result == NULL) {
        message("cannot determine current working directory.exit.");
        sleep(1);
        abort_cdargs();
    }
    /* this can be a memleak if not freed again */
    /* but then ... how long does cdargs usually run? */
    return strdup(buf);
}

string get_cwd_as_string(void) {
    char* cwd = get_cwd_as_charp();
    if(cwd == NULL) {
        return "";
    } else {
        string result = string(cwd);
        free(cwd);
        return result;
    }
}

string get_listfile(void) {
    string user="~";
    string file=DefaultListfile;
    string effective_listfile = "";
    if(opt_user.size() > 0) {
        user = opt_user;
    }
    if(opt_listfile.size() > 0) {
        return canonify_filename(opt_listfile);
    }
    if(user[0] == '~') {
        effective_listfile = user + "/" + file;
    } else {
        effective_listfile = "/home/" + user + "/" + file;
    }
    return canonify_filename(effective_listfile);
}

// return the last dir in path with a capital initial letter
string capitalized_last_dirname(string path) {
    string dirname = path.substr(path.find_last_of('/') + 1);
    dirname[0] = toupper(dirname[0]);
    return dirname;
}
// same but not capitalized
string last_dirname(string path) {
    // trailing slash
    if(path.at(path.size()-1) == '/') {
        path = path.substr(0,path.size()-1);
    }
    string dirname = path.substr(path.find_last_of('/') + 1);
    return dirname;
}

string canonify_filename(string filename) {

    size_t pos=0;
    // remove double slashes
    if((pos = filename.find("//",0)) < filename.size()) {
        filename.replace(pos,2,"/");
    }
   
    if (filename[0] != '~') {
        return filename;
    }
    // resolve home directory
    return string(getenv("HOME")) + filename.substr(1);
}

void version(void) {
    printf("cdargs - version %s\n", VERSION);
}

void usage(void) {
    printf("expanding the shell built-in cd with bookmarks and browser\n\n");
    printf("  Usually you don't call this programm directly.\n");
    printf("  Copy it somewhere into your path and create\n");
    printf("  a shell alias (which takes arguments) as described\n");
    printf("  in the file INSTALL which came with this program.\n");
    printf("\n  For general usage press 'H' while running.\n\n\n");
    printf("Usage:\n");
    printf("  cdargs              select from bookmarks\n");
    printf("  cdargs [--noresolve] <Needle>\n");
    printf("                      Needle is a filter for the list: each\n");
    printf("                      entry is compared to the Needle and if it\n");
    printf("                      doesn't match it won't show up.\n");
    printf("                      The Needle performs some magic here. See\n");
    printf("                      manpage for details.\n");
    printf("                      If --noresolve is given the list will be shown\n");
    printf("                      even if Needle matches a description exactly.\n");
    printf("  cdargs <digit>      Needle is a digit: make digit the current entry\n\n");
    printf("  cdargs [-a|--add]=[:desc:]path \n");
    printf("                              add PATH to the list file using the\n");
    printf("                              optional description between the colons\n");
    printf("                              and exit\n");
    printf("\n");
    printf("Other Options\n");
    printf("  -f, --file=PATH    take PATH as bookmark file\n"); 
    printf("  -u, --user=USER    read (default) bookmark file of USER\n"); 
    printf("  -o,- -output=FILE  use FILE as result file\n"); 
    printf("  -r, --nowrap       change the scrolling behaviour for long lists\n");
    printf("  -c, --cwd          make current directory the current entry if on the list\n");
    printf("  -b, --browse       start in BROWSE mode with the current dir\n"); 
    printf("  -h, --help         print this help message and exit\n"); 
    printf("  -v, --version      print version info and exit\n");
    printf("\n");
}

/***********************************************************/
/*                      CURSES STUFF                       */
/***********************************************************/

/* stuff to initialise the ncurses lib */
void init_curses(void) {
    initscr();               // init curses screen
    //savetty();             // ??
    nonl();                  // no newline
    cbreak();                // not in cooked mode: char immediately available
    noecho();                // chars are not echoed
    keypad(stdscr, true);    // Arrow keys etc
    leaveok(stdscr, TRUE);   // Don't show the...
    curs_set(0);             // ...cursor
    set_areas();
    curses_running = true;
}

void message(const char* msg) {
    if (curses_running) {
        move(msg_area_y, 0);
        clrtoeol();
        printw("%s [any key to continue]", msg);
        refresh();
        getch();
        move(msg_area_y, 0);
        clrtoeol();
    } else {
        printf("%s\n", msg);
    }
}

void display_list(void) {

    char description_format[40];

    vector<pair<string, string> > list;
    if (mode == LIST) {
        // perform some magic here: if the list contains just one
        // entry (probably due to filtering by giving a Needle)
        // we are done. 
        if(default_list.size() == 1 && !opt_no_resolve) {
            finish(current_entry(), true);
        } else {
            list = default_list;
        }
    } else {
        list = cur_list;
    }
    clear();
    update_modeline();
    int pos = display_area_ymin;
    if(CurrPosition > static_cast<int>(list.size()) - 1) {
        CurrPosition = list.size()-1;
    }

    // Calculate actual maxlength of descriptions so we can eliminate
    // trailing blanks. We have to iterate thru the list to get the
    // longest description
    unsigned int actual_maxlength = 0;
    if(mode == LIST) {
        for (listit li = list.begin() + yoffset;li != list.end(); ++li) {
            if ( strlen(li->first.c_str() ) > actual_maxlength ) {
                actual_maxlength = strlen(li->first.c_str() );
            }
        }
        // Don't let actual_maxlength > DESCRIPTION_MAXLENGTH
        if ( actual_maxlength > DESCRIPTION_MAXLENGTH) {
            actual_maxlength = DESCRIPTION_MAXLENGTH;
        }
    } else {
        actual_maxlength = DESCRIPTION_BROWSELENGTH;
    }
   
    string cwd  = get_cwd_as_string();
    for (listit li = list.begin() + yoffset;li != list.end(); ++li) {
        //string desc = li->first.substr(0, (DESCRIPTION_MAXLENGTH+1));
        string desc = li->first.substr(0, actual_maxlength);
        string path = li->second.substr(0, xmax - 16);
        string fullpath = li->second;
        char validmarker = ' ';
        if(!valid(fullpath,PATH_IS_DIR)) {
            validmarker = '!';
        }
        if (pos > display_area_ymax) {
            break;
        }
        move(pos, 0);
        if (pos == CurrPosition - yoffset) {
            attron(A_STANDOUT);
        }
        if (fullpath == cwd) {
            attron(A_BOLD);
        }
        if (pos >= shorties_offset && pos < 10+shorties_offset) {
            printw("%2d", pos-shorties_offset);
        } else {
            printw("  ");
        }
      
        // Compose format string for printw. Notice %% to represent literal %
        sprintf(description_format, " [%%-%ds] %%c%%s", actual_maxlength );
      
        printw(description_format, desc.c_str(), validmarker,path.c_str());
        if (pos == CurrPosition - yoffset) {
            attroff(A_STANDOUT);
        }
        if (fullpath == cwd) {
            attroff(A_BOLD);
        }
        pos++;
    }      
}

void update_modeline(void) {
    move(modeliney, 0);
    clrtoeol();
    move(modeliney, 0);
    attron(A_REVERSE);
    string curmode;
    char modechar;
    if (mode == LIST) {
        curmode = "List";
        modechar = 'L';
    } else {
        curmode = "Browse";
        modechar = 'B';
    }
    string cwd = get_cwd_as_string();
    attron(A_BOLD);
    printw("%c: %s ", modechar, cwd.c_str());
//   printw(" yoff: %d shoff: %d currpos: %d ", yoffset,shorties_offset,CurrPosition);
    attroff(A_BOLD);
    hline(' ', xmax);
    attroff(A_REVERSE);
}

void set_areas(void) {
    getmaxyx(stdscr, terminal_height, terminal_width); // get win-dims
    xmax = terminal_width - 1;
    display_area_ymin = 0;
    display_area_ymax = terminal_height - 3;
    modeliney = terminal_height - 2;
    msg_area_y = terminal_height - 1;
}

void resizeevent(int sig) {
    // shut up, compiler
    (void)sig;  
    // re-connect
    signal(SIGWINCH, resizeevent);
    // FIXME: is this the correct way??
    endwin();
    refresh();
    init_curses();
    display_list();
}

bool visible(int pos) {
    if(pos < 0) {
        return false;
    }
    if(pos-yoffset<0) {
        return false;
    }
    if(pos+yoffset > display_area_ymax) {
        return false;
    }
    return true;
}

int max_yoffset(void) {
    int len, ret;
    if (mode == BROWSE) {
        len = cur_list.size() - 1;
    } else {
        len = default_list.size() - 1;
    }
    ret = len - display_area_ymax;
    return ret<0 ? 0:ret;
}


void helpscreen(void) {
    char *pager = getenv("PAGER");
    if (!pager) {
        // FIXME:  how to detect debian and the /usr/bin/pager rule?
        struct stat buf;
        if(stat("/usr/bin/pager",&buf) == 0) {
            pager = "/usr/bin/pager";
        } else {
            if(stat("/usr/bin/less",&buf) == 0) {
                pager = "/usr/bin/less";
            } else {
                pager = "more";
            }
        }
    }
    endwin();
    FILE* help = popen(pager,"w");
// FIXME: error checking this doesn't work, er?
    if(help == NULL) {
        init_curses();
        message("could not open pager");
        return;
    }
   
    int l = 0;
    char* help_lines[] = {
        "cdargs (c) 2001-2003 S. Kamphausen <http://www.skamphausen.de>",
        "<UP>/<DOWN>  move selection up/down and scroll",
        "             please see manpage for alternative bindings!",
        "<ENTER>      select current entry",
        "<TAB>        toggle modes: LIST or BROWSE",
        "<HOME>/<END> goto first/last entry in list if available in terminal",
        "c            add current directory to list",
        "C            same as 'c' but ask for a description",
        "<PgUP/Dn>    scroll the list 10 lines up or down",
        "e,v          edit the list in $EDITOR",
        "H,?          show this help",
        "~,/          browse home/root directory",
        "q            quit - saving the list",
        "C-c,C-g,C-[  abort - don't save the list",
        "             ",
        "Commands in BROWSE-mode:",
        "<LEFT>       descent into current directory",
        "<RIGHT>      up one directory",
        "[num]        make [num] the highlighted entry",
        "a            add current entry to list",
        "A            same as 'a' but ask for a description",
        ".            toggle display of hidden files",
        "             ",
        "Commands in LIST-mode:",
        "[num]        select and resolve entry [num] if displayed",
        "<LEFT>       descent into the current entry",
        "<RIGHT>      up one directory from current dir",
        "d            delete current entry from list",
        "a            add current directory to list",
        "s,t          swap(transpose) two lines in the list",
        "M/m          move an entry up down in the list"
    };
    int help_lines_size = 31;
    while (l < help_lines_size) {
        fprintf(help,"%s\n",help_lines[l]);
        l++;
    }
    fprintf(help,"\n");
    pclose(help);
    // convenience for more which exits at the end:
    if(strstr(pager,"more")) {
        sleep(1);
    }
    refresh();
    init_curses();
}   

/************************************************/
/*                     EXITs                    */
/************************************************/

void fatal_exit(char* msg) {
    endwin();
    fprintf(stderr, msg);
    exit(1);
}

void terminate(int sig) {
    endwin();
    if(sig == 11) {
        fprintf(stderr,"programm received signal %d\n",sig);
        fprintf(stderr,"This should never happen.\n");
        fprintf(stderr,"Maybe you want to send an email to the author and report a bug?\n");
        fprintf(stderr,"Author: Stefan Kamphausen <http://www.skamphausen.de>\n"); 
    }
    fprintf(stderr,"abort.\n");
    exit(1);
}

void finish(string result, bool retval) {

    curs_set(1); // re-show cursor (FIXME: necessary?)
    clear();     // clear screen for stupid terminals like aterm
    refresh();   // ..and make sure we really see the clean screen
    //resetty();   // ??
    endwin();    // finish the curses at all
   
    // only save if list was not filtered!
    if (!NeedleGiven){
        list_to_file();
    }

    // is the result really a dir?
    if(!valid(result,PATH_IS_DIR)) {
        fprintf(stderr,"This is not a valid directory:\n%s\n",result.c_str());
        exit(-3);
    }
//    string resfile = canonify_filename(Resultfile);
    string resfile = canonify_filename(get_resultfile());
    ofstream out(resfile.c_str());
    if (out) {
        out << result << endl;
        out.close();
    }
    if (!retval) {
        exit(1);
    }
    exit(0);
}
void abort_cdargs(void) {
    curs_set(1); // re-show cursor (FIXME: necessary?)
    clear();     // clear screen for stupid terminals like aterm
    refresh();   // ..and make sure we really see the clean screen
    //resetty();   // ??
    endwin();    // finish the curses at all
    exit(-1);
}

bool valid(string path, pathtype mode) {
    struct stat buf;
    string canon_path = canonify_filename(path);
   
    if(mode == PATH_IS_FILE) {
        stat(canon_path.c_str(), &buf);
        if (S_ISREG(buf.st_mode)) return true;
    } else if (mode == PATH_IS_DIR) {
        stat(canon_path.c_str(), &buf);
        if (S_ISDIR(buf.st_mode)) return true;
    }
    return false;
}
