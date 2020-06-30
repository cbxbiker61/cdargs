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

#include <config.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <map>
using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#if defined(HAVE_NCURSES_H)
#include <ncurses.h>
#else
#include <curses.h>
#endif

#define DEBUG 0

// thanks to cscope source for this:
#define CTRL(x) (x & 037)
#if (BSD || V9) && !__NetBSD__
#define TERMINFO 0   /* no terminfo curses */
#else
#define TERMINFO 1
#endif

namespace {

string getCwd(void)
{
	char buf[PATH_MAX];
	const char *result(getcwd(buf, sizeof(buf)));

	return ( result ) ? result : "";
}

string canonifyFilename(string filename)
{
	size_t pos(0);

	// remove double slashes
	if ( (pos = filename.find("//", 0)) < filename.size() )
		filename.replace(pos, 2, "/");

	return ( filename[0] == '~' )
				? string(getenv("HOME")) + filename.substr(1)
				: filename;
}

enum DisplayBrowseModes
{
	LIST
	, BROWSE
};

int mode = LIST;
bool showHiddenFiles;
bool isListfileEmpty;

// Change this if you want different length (actually width) for the
// description:
const int DESC_MAX_LEN(18);
const int DESC_BROWSE_LEN(8);

const char *Needle;
bool isNeedleGiven;
int currentPos;
map<string, int> lastPositionMap;

class Options
{
public:
	Options() : _noWrap(false), _noResolve(false), _cwdIsCurrent(false), _user(), _listFile(), _resultFile() {}

	void setNoWrap(bool v) { _noWrap = v; }
	bool getNoWrap(void) const { return _noWrap; }
	void setNoResolve(bool v) { _noResolve = v; }
	bool getNoResolve(void) const { return _noResolve; }
	void setCwdIsCurrent(bool v) { _cwdIsCurrent = v; }
	bool getCwdIsCurrent(void) const { return _cwdIsCurrent; }
	void setUser(const string &s) { _user = s; }
	const string &getUser(void) const { return _user; }

	void setListFile(const string &s) { _listFile = s; }
	string getListFile(bool wantOld = false)
	{
		if ( ! _listFile.empty() )
			return canonifyFilename(_listFile);

		const string def( ( ! wantOld ) ? DEFAULT_LIST_FILE : DEFAULT_LIST_FILE_OLD);
		const string user( (! _user.empty()) ? _user : string("~"));
		const string s( ( user[0] == '~' )
						? user + "/" + def
						: string("/home/") + user + "/" + def);

		return canonifyFilename(s);
	}


	void setResultFile(const string &s) { _resultFile = s; }
	const string &getResultFile(void) const
	{
		return ( ! _resultFile.empty() ) ? _resultFile : DEFAULT_RESULT_FILE;
	}

	const static string DEFAULT_LIST_FILE;
	const static string DEFAULT_LIST_FILE_OLD;

private:
	bool _noWrap;
	bool _noResolve;
	bool _cwdIsCurrent;
	string _user;
	string _listFile;
	string _resultFile;
	const static string DEFAULT_RESULT_FILE;

} options;

const string Options::DEFAULT_LIST_FILE(".config/cdargs");
const string Options::DEFAULT_LIST_FILE_OLD(".cdargs");
const string Options::DEFAULT_RESULT_FILE("~/.cdargsresult");

void showVersion(void)
{
	cout << "cdargs " VERSION << endl;
}

void showUsage(void)
{
	cout <<
"expanding the shell built-in cd with bookmarks and browser\n\n"
"  Usually you don't call this programm directly.\n"
"  Copy it somewhere into your path and create\n"
"  a shell alias (which takes arguments) as described\n"
"  in the file INSTALL which came with this program.\n"
"\n  For general usage press 'H' while running.\n\n\n"
"Usage:\n"
"  cdargs              select from bookmarks\n"
"  cdargs [--noresolve] <Needle>\n"
"                      Needle is a filter for the list: each\n"
"                      entry is compared to the Needle and if it\n"
"                      doesn't match it won't show up.\n"
"                      The Needle performs some magic here. See\n"
"                      manpage for details.\n"
"                      If --noresolve is given the list will be shown\n"
"                      even if Needle matches a description exactly.\n"
"  cdargs <digit>      Needle is a digit: make digit the current entry\n\n"
"  cdargs [-a|--add]=[:desc:]path \n"
"                              add PATH to the list file using the\n"
"                              optional description between the colons\n"
"                              and exit\n"
"\n"
"Other Options\n"
"  -f, --file=PATH    take PATH as bookmark file\n"
"  -u, --user=USER    read (default) bookmark file of USER\n"
"  -o, --output=FILE  use FILE as result file\n"
"  -r, --nowrap       change the scrolling behaviour for long lists\n"
"  -c, --cwd          make current directory the current entry if on the list\n"
"  -b, --browse       start in BROWSE mode with the current dir\n"
"  -h, --help         print this help message and exit\n"
"  -v, --version      print version info and exit\n"
	<< endl;
}

class Lists
{
public:
	Lists() : _current(), _default() {}

	typedef vector<pair<string, string> > ListType;

	ListType &getCurrent(void) { return _current; }
	ListType &getDefault(void) { return _default; }

private:
	ListType _current; // probably directory listing
	ListType _default; // read from and saved to file
} lists;

// terminal coordinates and other curses stuff
class TermInfo
{
public:
	void initCurses(void);
	void showMessage(const string &msg);
	string getDescFromUser(void);
	void updateModeline(void);

	bool isVisible(int pos)
	{
		return ( pos >= 0 ) && ( pos - yOffset >= 0 ) && ( pos + yOffset <= dspArea.yMax );
	}

	int termWidth;
	int termHeight;
	int xMax;

	struct
	{
		int yMax;
		int yMin;
	} dspArea;

	int  modelineY;
	int  msgAreaY;
	int  yOffset;
	bool isCursesRunning;
	int shortiesOffset;

private:
	void setAreas(void);
};

void TermInfo::initCurses(void)
{
	initscr(); // init curses screen
	//savetty(); // ??
	nonl(); // no newline
	cbreak(); // not in cooked mode: char immediately available
	noecho(); // chars are not echoed
	keypad(stdscr, true); // Arrow keys etc
	leaveok(stdscr, TRUE); // Don't show the...
	curs_set(0); // ...cursor
	setAreas();
	isCursesRunning = true;
}

void TermInfo::setAreas(void)
{
	getmaxyx(stdscr, termHeight, termWidth); // get win-dims
	xMax = termWidth - 1;
	dspArea.yMin = 0;
	dspArea.yMax = termHeight - 3;
	modelineY = termHeight - 2;
	msgAreaY = termHeight - 1;
}

void TermInfo::showMessage(const string &msg)
{
	if ( isCursesRunning )
	{
		move(msgAreaY, 0);
		clrtoeol();
		printw("%s [any key to continue]", msg.c_str());
		refresh();
		getch();
		move(msgAreaY, 0);
		clrtoeol();
	}
	else
	{
		cout << msg << endl;
	}
}

string TermInfo::getDescFromUser(void)
{
	move(msgAreaY, 0);
	clrtoeol();
	printw("Enter description (<ENTER> to quit): ");
	refresh();
	echo();

	char desc[DESC_MAX_LEN + 1];
	getnstr(desc, DESC_MAX_LEN);
	noecho();
	move(msgAreaY, 0);
	clrtoeol();

	return string(desc);
}

void TermInfo::updateModeline(void)
{
	move(modelineY, 0);
	clrtoeol();
	move(modelineY, 0);
	attron(A_REVERSE);

	const char modechar(( mode == LIST ) ? 'L' : 'B');
	const string cwd(getCwd());

	attron(A_BOLD);
	printw("%c: %s ", modechar, cwd.c_str());
	//   printw(" yoff: %d shoff: %d currpos: %d ", term.yOffset, term.shortiesOffset, currentPos);
	attroff(A_BOLD);
	hline(' ', xMax);
	attroff(A_REVERSE);
}

TermInfo term;

void abortProgram(void) __attribute__((noreturn));

void abortProgram(void)
{
	curs_set(1); // re-show cursor (FIXME: necessary?)
	clear(); // clear screen for stupid terminals like aterm
	refresh(); // ..and make sure we really see the clean screen
	//resetty();   // ??
	endwin(); // finish the curses at all
	exit(-1);
}

void fatalExit(const string &msg)
{
	endwin();
	cerr << msg << endl;
	exit(1);
}

void terminate(int sig)
{
	endwin();

	if ( sig == 11 )
	{
		cerr << "programm received signal " << sig << endl
			<< "This should never happen." << endl
			<< "Maybe you want to send an email to the author and report a bug?" << endl
			<< "Author: Stefan Kamphausen <http://www.skamphausen.de>\n" << endl;
	}

	cerr << "abort." << endl;
	exit(1);
}

void listToFile(void)
{
	// don't write empty files FIXME: unlink file?
	const string listFile(options.getListFile());

	if ( lists.getDefault().empty() )
	{
		unlink(listFile.c_str());
		return;
	}

	if ( ! options.getUser().empty() ) // don't touch the listfile of another user
		return;

	if ( rename(listFile.c_str(), (listFile + "~").c_str()) == -1 )
		cerr << "warning: can't create backupfile" << endl;

	ofstream ofs(listFile.c_str());

	if ( ofs )
	{
		for ( auto &o : lists.getDefault() )
			ofs << o.first << " " << o.second << endl;
	}
	else
	{
		fatalExit("can't write listfile");
	}
}

bool isValidFileOrDir(const string &path, bool isDir)
{
	struct stat buf;
	stat(canonifyFilename(path).c_str(), &buf);

	return ( isDir ) ? S_ISDIR(buf.st_mode) : S_ISREG(buf.st_mode);
}

inline bool isValidFile(const string &path)
{
	return isValidFileOrDir(path, false);
}

inline bool isValidDir(const string &path)
{
	return isValidFileOrDir(path, true);
}

void finish(const string &result, bool retval)
{
	curs_set(1); // re-show cursor (FIXME: necessary?)
	clear(); // clear screen for stupid terminals like aterm
	refresh(); // ..and make sure we really see the clean screen
	//resetty();   // ??
	endwin(); // finish the curses at all

	// only save if list was not filtered!
	if ( ! isNeedleGiven )
		listToFile();

	if ( ! isValidDir(result) )
	{
		cerr << "This is not a valid directory:" << endl << result << endl;
		exit(-3);
	}

	ofstream ofs(canonifyFilename(options.getResultFile()).c_str());

	if ( ofs )
	{
		ofs << result << endl;
		ofs.close();
	}

	exit(retval ? 0 : 1);
}

bool isNoShow(const string &name)
{
	if ( mode == BROWSE )
	{
		if ( name == ".." || ( ! showHiddenFiles && name[0] == '.' ) )
			return true;
	}
	else
	{
		if ( ! isNeedleGiven || ! Needle )
			return false;

		// TODO: this should be case-insentive and should leverage string class
		return ! strstr(name.c_str(), Needle);
	}

	return false;
}

string getLastDirName(const string &path)
{
	const string s( (path.at(path.size() - 1) != '/' )
						? path
						: path.substr(0, path.size() - 1));

	return s.substr(s.find_last_of('/') + 1);
}

void listFromDir(const char *name = ".")
{
	string prevDir;

	if ( string(name) == ".." )
		prevDir = getCwd();

	// Checking, Changing and Reading
	if ( chdir(name) < 0 )
		term.showMessage(string("couldn't change to dir: ") + name);

	string cwd(getCwd());
	DIR *THISDIR(opendir(cwd.c_str()));

	if ( ! THISDIR )
	{
		term.showMessage(string("couldn't read dir: ") + cwd);
		sleep(1);
		abortProgram();
	}

	term.yOffset = 0;
	term.shortiesOffset = 1;
	struct dirent *en;
	struct stat buf;

	lists.getCurrent().clear();

	currentPos = 1;

	// put the current directory on top ef every listing
	string desc(".");
	lists.getCurrent().push_back(make_pair(desc, cwd));

	// read home dir
	while ( (en = readdir(THISDIR)) )
	{
		/* filter dirs */
		if ( isNoShow(en->d_name) )
			continue;

		const string fullname(canonifyFilename(cwd + string("/") + string(en->d_name)));

		stat(fullname.c_str(), &buf);

		if ( ! S_ISDIR(buf.st_mode) )
			continue;

		lists.getCurrent().push_back(make_pair(getLastDirName(fullname), fullname));
	}

	// empty directory listing: .. and .
	desc = "..";
	string path(getCwd());

	if ( lists.getCurrent().size() == 1 )
	{
		path = path.substr(0, path.find(getLastDirName(path)));
		lists.getCurrent().push_back(make_pair(desc,path));
	}

	sort(lists.getCurrent().begin(), lists.getCurrent().end());

	// have we remembered a current position for this dir?
	if ( lastPositionMap.count(cwd) )
	{
		currentPos = lastPositionMap[cwd];
	}
	else
	{
		//currentPos = 1;
		if ( ! prevDir.empty() )
		{
			int count(0);

			for ( auto &o : lists.getCurrent() )
			{
				if ( o.second == prevDir )
				{
					currentPos = count;
					break;
				}

				++count;
			}
		}
	}

	if ( ! term.isVisible(currentPos) )
		term.yOffset = currentPos - 1;

	closedir(THISDIR);
}

void toggleHidden(void)
{
	showHiddenFiles = ! showHiddenFiles;

	if ( mode == BROWSE )
		listFromDir();
}

bool listFromFile(void)
{
	int linecount(0);
	lists.getDefault().clear();
	ifstream ifs(options.getListFile().c_str());

	if ( ifs )
	{
		const string cwd(getCwd());

		while ( ! ifs.eof() )
		{
			string line;

			getline(ifs, line);

			if ( line.empty() )
				continue;

			// comments ... are not saved later so we simply don't allow
			// them
			// if (line[0] == '#') {
			//    continue;
			// }
			// detect path and description at the leading slash of the
			// absolute path:
			const string desc = line.substr(0, line.find('/') - 1);
			const string path = line.substr(line.find('/'));

			if ( options.getCwdIsCurrent() && (cwd == path) )
				currentPos = linecount;

			// counting the lines: if only one, no resolving should take place
			++linecount;

			/*
			 * if we have an exact match and not options.getNoResolve(),
			 * the first entry is the result!
			 */
			if ( isNeedleGiven && Needle && Needle == desc && ! options.getNoResolve() )
				finish(path, true);

			if ( isNoShow(desc) && isNoShow(path) ) // filtered?
				continue;

			lists.getDefault().push_back(make_pair(desc, path));
		}

		ifs.close();

		// some magic:
		switch ( linecount )
		{
		case 0:
			isListfileEmpty = true;
			break;
		case 1:
			options.setNoResolve(true);
			break;
		}

		term.shortiesOffset = 0;

		return ! lists.getDefault().empty();
	}

	return false;
}

void toggleMode(void)
{
	if ( mode == LIST )
	{
		mode = BROWSE;
		listFromDir(".");
	}
	else
	{
		if ( isListfileEmpty ) // list was empty at start
		{
			if ( ! lists.getDefault().empty() ) // but isn't now
			{
				listToFile();
				listFromFile();
				mode = LIST;
			}
			else // ok, we have nothing to show here
			{
				term.showMessage("No List entry. Staying in BROWSE mode");
				mode = BROWSE;
				listFromDir(".");
			}
		}
		else // the "normal" case;
		{
			// disable Needle and reload the list!
			isNeedleGiven = false;
			Needle = nullptr;
			listFromFile();
			term.yOffset = 0;
			term.shortiesOffset = 0;
			currentPos = 0;
			mode = LIST;
		}
	}
}

bool entryNumberExists(unsigned int n)
{
	if ( mode == LIST )
		return ( n < lists.getDefault().size() );
	else if ( mode == BROWSE )
		return ( n < lists.getCurrent().size() );

	return false;
}

int getMaxYOffset(void)
{
	const int len((mode == BROWSE) ? lists.getCurrent().size() - 1 : lists.getDefault().size() - 1);
	const int ret(len - term.dspArea.yMax);

	return (ret < 0) ? 0 : ret;
}

void adjustCurrentPos(int n = 0, bool wraparound = true)
{
	int newPos(currentPos + n);
	int max( (mode == LIST) ? lists.getDefault().size() - 1 : lists.getCurrent().size() - 1);
	int min(0);

	if ( newPos < min )
	{
		if ( options.getNoWrap() || ! wraparound )
			return;

		newPos = max;
		term.yOffset = getMaxYOffset();
	}
	else if ( newPos > max )
	{
		if ( options.getNoWrap() || ! wraparound )
			return;

		newPos = min;
		term.yOffset = 0;
	}

	currentPos = newPos;

	// scrolling...
	if ( currentPos - term.yOffset >= term.dspArea.yMax )
		++term.yOffset;

	if ( term.yOffset > 0 && currentPos == term.yOffset )
		--term.yOffset;
}

string getCurrentEntry(void)
{
	string res;

	if ( mode == LIST )
	{
		if ( lists.getDefault().empty() )
			return string(".");

		res = lists.getDefault()[currentPos].second;
	}
	else if ( mode == BROWSE )
	{
		if ( lists.getCurrent().empty() )
			return string(".");

		res = lists.getCurrent()[currentPos].second;
	}

	return res.empty() ? string(".") : res;
}

void showHelpScreen(void)
{
	const char *pager(getenv("PAGER"));

	if ( ! pager )
	{
		// FIXME:  how to detect debian and the /usr/bin/pager rule?
		struct stat buf;

		if ( ! stat("/usr/bin/pager",&buf) )
			pager = "/usr/bin/pager";
		else if ( ! stat("/usr/bin/less",&buf) )
			pager = "/usr/bin/less";
		else if ( ! stat("/usr/bin/more",&buf) )
			pager = "/usr/bin/more";
		else
			pager = "more";
	}

	endwin();

	FILE *helper(popen(pager, "w"));

// FIXME: error checking this doesn't work, er?
	if ( ! helper )
	{
		term.initCurses();
		term.showMessage("could not open pager");
		return;
	}

	const char *helpLines[] = {
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

	const size_t helpLinesCount(31);
	size_t l(0);

	while ( l < helpLinesCount )
	{
		fprintf(helper, "%s\n", helpLines[l]);
		++l;
	}

	fprintf(helper, "\n");
	pclose(helper);

	// convenience for more which exits at the end:
	if ( strstr(pager, "more") )
		sleep(1);

	refresh();
	term.initCurses();
}

void deleteFromDefaultList(int pos)
{
	int count(0);
	auto &def(lists.getDefault());

	for ( auto li = def.begin(); li != def.end(); ++li )
	{
		if ( count == pos )
		{
			def.erase(li);
			adjustCurrentPos(0, false);
			// we want no one-entry-in-list-magic anymore
			options.setNoResolve(true);
			return;
		}

		++count;
	}
}

void addToDefaultList(string path, const string &description = "", bool askForDesc = false)
{
	if ( lists.getCurrent().empty() && term.isCursesRunning )
		path = getCwd();

	// get the description (either from user or generic)
	string desc;

	if ( description.empty() )
	{
		if ( askForDesc && term.isCursesRunning )
		{
			desc = term.getDescFromUser();

			if ( desc.empty() ) // empty string is quit
				return;

		}
		else
		{
			desc = getLastDirName(path);
		}
	}
	else
	{
		desc = description;
	}

	lists.getDefault().push_back(make_pair(desc, path));
	term.showMessage(string("added :") + desc + ":" + path);
}

void displayList(void)
{
	Lists::ListType list;

	if ( mode == LIST )
	{
		// perform some magic here: if the list contains just one
		// entry (probably due to filtering by giving a Needle)
		// we are done.
		if ( lists.getDefault().size() == 1 && ! options.getNoResolve() )
			finish(getCurrentEntry(), true);
		else
			list = lists.getDefault();
	}
	else
	{
		list = lists.getCurrent();
	}

	clear();
	term.updateModeline();

	if ( currentPos > static_cast<int>(list.size()) - 1 )
		currentPos = list.size() - 1;

	// Calculate actual maxlength of descriptions so we can eliminate
	// trailing blanks. We have to iterate thru the list to get the
	// longest description
	unsigned int actualMaxLen(0);

	if ( mode == LIST )
	{
		for ( auto li = list.begin() + term.yOffset; li != list.end(); ++li )
		{
			if ( strlen(li->first.c_str() ) > actualMaxLen )
				actualMaxLen = strlen(li->first.c_str() );
		}

		if ( actualMaxLen > DESC_MAX_LEN )
			actualMaxLen = DESC_MAX_LEN;
	}
	else
	{
		actualMaxLen = DESC_BROWSE_LEN;
	}

	const string cwd(getCwd());
	int pos(term.dspArea.yMin);

	for ( auto li = list.begin() + term.yOffset; li != list.end(); ++li )
	{
		if ( pos > term.dspArea.yMax )
			break;

		move(pos, 0);

		if ( pos == currentPos - term.yOffset )
			attron(A_STANDOUT);

		const string fullpath = li->second;

		if ( fullpath == cwd )
			attron(A_BOLD);

		if ( pos >= term.shortiesOffset && (pos < 10 + term.shortiesOffset) )
			printw("%2d", pos - term.shortiesOffset);
		else
			printw("  ");

		const string desc = li->first.substr(0, actualMaxLen);
		const string path = li->second.substr(0, term.xMax - 16);
		char validMarker = isValidDir(fullpath) ? ' ' : '!';
		char fmt[40];

		snprintf(fmt, sizeof(fmt), " [%%-%ds] %%c%%s", actualMaxLen);
		printw(fmt, desc.c_str(), validMarker, path.c_str());

		if ( pos == currentPos - term.yOffset )
			attroff(A_STANDOUT);

		if ( fullpath == cwd )
			attroff(A_BOLD);

		++pos;
	}
}

void editListfile(void)
{
	const char *editor(getenv("EDITOR"));

	if ( ! editor )
	{
		struct stat buf;

		editor = ( ! stat("/usr/bin/editor", &buf) ? "/usr/bin/editor" : "vi");
	}

	endwin();
	listToFile();
	system((string(editor) + " \"" + options.getListFile() + "\"").c_str());
	listFromFile();
	refresh();
	term.initCurses();
	displayList();
}

void
swapTwoEntries(int advanceAfterwards)
{
	const int idx( ( advanceAfterwards >= 0 ) ? currentPos + 1 : currentPos - 1);

	if ( idx > 0 && idx < int(lists.getDefault().size())
			&& mode == LIST && ! isNeedleGiven )
	{
		const auto tmp(lists.getDefault()[currentPos]);
		lists.getDefault()[currentPos] = lists.getDefault()[idx];
		lists.getDefault()[idx] = tmp;
		adjustCurrentPos(advanceAfterwards);
		displayList();
	}
	else
	{
		beep();
	}
}

bool userInteraction(int c)
{
	int num;
	string curen;

	switch ( c )
	{
	// ==== Exits
	case CTRL('['): // vi
	case CTRL('g'): // emacs
		abortProgram();
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
		num =  c - '0' + term.shortiesOffset;

		if ( mode == LIST )
		{
			if ( entryNumberExists(num) )
			{
				currentPos = num + term.yOffset;
				return options.getNoResolve();
			}
		}
		else
		{
			currentPos = num + term.yOffset;
			return true;
		}
		break;

	// ==== Modes
	case '.':
		// show hidden files
		toggleHidden();
		break;
	case '\t': // TAB
		toggleMode();
		break;

		// ==== Navigate The List
	case 'j': // vi
	case CTRL('n'): // emacs
#if TERMINFO
	case KEY_DOWN:
#endif
		// navigate list++
		adjustCurrentPos(+1);
		break;

	case 'k': // vi
	case CTRL('p'): // emacs
#if TERMINFO
	case KEY_UP:
#endif
		// navigate list--
		adjustCurrentPos(-1);
		break;

	case '^': // vi?
	case CTRL('a'): // emacs
#if TERMINFO
	case KEY_HOME:
#endif
		// go to top
		currentPos = 0;
		term.yOffset = 0;
		break;

	case '$': // vi
	case CTRL('e'): // emacs
# if TERMINFO
	case KEY_END:
#endif
		// go to end
		currentPos = ( mode == BROWSE )
							? lists.getCurrent().size() - 1
							: lists.getDefault().size() - 1;

		term.yOffset = getMaxYOffset();
		break;

		//==== move the shortcut digits ('shorties')
		// FIXME case ??:  //vi
		// FIXME maybe change the scrolling behaviour to not take the window but the whole
		// list? That means recentering when shorties leave the screen (adjust yoffset and
		// currentPos)

	case CTRL('v'): // emacs
#if TERMINFO
	case KEY_NPAGE:
#endif
		for ( int i = 0; i < 10; ++i )
			adjustCurrentPos(+1, false);

		break;

	// fixme: vi?
	//case CTRL(''): // FIXME: META(x)??
#if TERMINFO
	case KEY_PPAGE:
#endif
		for ( int i = 0; i < 10; ++i )
			adjustCurrentPos(-1, false);

		break;

	case 'h': // vi
	case CTRL('b'): // emacs
#if TERMINFO
	case KEY_LEFT:
#endif
		// up dir
		if ( mode == BROWSE )
			lastPositionMap[getCwd()] = currentPos;
		else
			mode = BROWSE;

		listFromDir("..");
		break;

	case 'l': // vi
	case CTRL('f'): // emacs
#if TERMINFO
	case KEY_RIGHT:
#endif
		// descend dir at cur pos
		curen = getCurrentEntry();

		if ( mode == BROWSE )
			lastPositionMap[getCwd()] = currentPos;
		else
			mode = BROWSE;

		listFromDir(curen.c_str());
		break;

	case 'H':
	case '?':
		showHelpScreen();
		break;

	case 'd':
	case CTRL('d'): // emacs
#if TERMINFO
	case KEY_BACKSPACE:
#endif
		// delete dir acp
		if ( mode == LIST && ! isNeedleGiven )
			deleteFromDefaultList(currentPos);
		else
			beep();

		break;

#if TERMINFO
	case KEY_IC:
#endif
	case 'a':
		// add dir acp (if in browse mode)
		if ( ! isNeedleGiven )
			addToDefaultList(( mode == BROWSE ) ? getCurrentEntry() : getCwd());
		else
			beep();

		break;

	case 'A':
		if ( ! isNeedleGiven )
			// add dir acp (if in browse mode) (ask for desc)
			addToDefaultList(( mode == BROWSE ) ?  getCurrentEntry() : getCwd() , "", true);
		else
			beep();

		break;

	case 'c':
		if ( ! isNeedleGiven )
			addToDefaultList(getCwd());
		else
			beep();

		break;

	case 'C':
		if ( ! isNeedleGiven )
			addToDefaultList(getCwd(), "", true);
		else
			beep();

		break;

	// ==== EDIT the list
	case 'v':
	case 'e':
	{
		if ( ! isNeedleGiven )
			editListfile();
		else
			beep();

		break;
	}

	case 'm':
		swapTwoEntries(1);
		break;

	case 'M':
		swapTwoEntries(-1);
		break;

	case 't':
	case 's':
		swapTwoEntries(0);
		break;

	// ==== Filesystem Hotspots
	case '~':
		mode = BROWSE;
		listFromDir(getenv("HOME"));
		break;

	case '/':
		mode = BROWSE;
		listFromDir("/");
		break;

	default:
		beep();
		term.showMessage("unknown command");
	}

	return true;
}

void addToListfile(string path)
{
	if ( path.at(0) == '=' )
		path = path.substr(1);

	// the syntax for passing descriptions from the command line is:
	// --add=:desc:/absolute/path
	string desc;

	if ( path.at(0) == ':' )
	{
		const int colon2Pos(path.find(":", 1));

		if ( colon2Pos > (DESC_MAX_LEN + 1) )
		{
			cerr << "description too long! max "
					<< DESC_MAX_LEN << " chars" << endl;
			exit(-4);
		}
		else
		{
			desc = path.substr(1, colon2Pos - 1);
		}

		 path = path.substr(colon2Pos + 1);
	}

	// FIXME: check for existance here?
	if ( path.at(0) != '/' )
	{
		cerr << "this is not an absolute path:\n" << path << endl;
		exit(-2);
	}

	listFromFile();
	addToDefaultList(path, desc, false);
	listToFile();
}

void handleResizeEvent(int sig __attribute__((unused)))
{
	// re-connect
	signal(SIGWINCH, handleResizeEvent);
	// FIXME: is this the correct way??
	endwin();
	refresh();
	term.initCurses();
	displayList();
}

void convertOldList(void)
{
	const string newList(options.getListFile(false));
	const string oldList(options.getListFile(true));

	if ( ! isValidFile(newList) && isValidFile(oldList) )
	{
		rename(oldList.c_str(), newList.c_str());
		unlink((oldList + "~").c_str());
	}
}

} // namespace

int main(int argc, char **argv)
{
	convertOldList();

	for ( ; ; )
	{
		static struct option longOpts[] =
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

		int optIdx = 0;
		int c = getopt_long(argc, argv, "a:f:u:brco:vh", longOpts, &optIdx);

		if ( c == -1 )
			break;

		string optname;
		string argument;

		switch ( c )
		{
		case 0:
			optname = string(longOpts[optIdx].name);

			if ( optname == "help" )
			{
				showVersion();
				showUsage();
				exit(0);
			}
			else if ( optname == "version" )
			{
				showVersion();
				exit(0);
			}
			else if ( optname == "add" )
			{
				argument = string(optarg);
				addToListfile(argument);
				exit(0);
			}
			else if ( optname == "file" )
				options.setListFile(optarg);
			else if ( optname == "user" )
				options.setUser(optarg);
			else if ( optname == "browse" )
				mode = BROWSE;
			else if ( optname == "nowrap" )
				options.setNoWrap(true);
			else if ( optname == "noresolve" )
				options.setNoResolve(true);
			else if ( optname == "cwd" )
				options.setCwdIsCurrent(true);
			else if ( optname == "output" )
				options.setResultFile(optarg);

			break;

		case 'a':
			argument = string(optarg);
			addToListfile(argument);
			exit(0);
			break;

		case 'f':
			options.setListFile(optarg);
			break;

		case 'u':
			options.setUser(optarg);
			break;

		case 'b':
			mode = BROWSE;
			break;

		case 'r':
			options.setNoResolve(true);
			break;

		case 'c':
			options.setCwdIsCurrent(true);
			break;

		case 'o':
			options.setResultFile(optarg);
			break;

		case 'h':
			showVersion();
			showUsage();
			exit(0);
			break;

		case 'v':
			showVersion();
			exit(0);

		default:
			showUsage();
			exit(1);
		}
	}

	if ( optind < argc )
	{
		Needle = argv[optind];
		const int len(strlen(Needle));

		if ( len > 0 )
			isNeedleGiven = true;

		if ( len == 1 && isdigit(Needle[0]) )
		{
			currentPos = atoi(Needle);
			Needle = nullptr;
		}
	}

	// leave terminal tidy
	// FIXME: what other signals do I need to catch?
	signal(SIGINT, terminate);
	signal(SIGTERM, terminate);
	signal(SIGSEGV, terminate);
	term.initCurses();
	// answer to terminal reizing
	signal(SIGWINCH, handleResizeEvent);

	/* get list from file or start in browse mode */
	if ( ! listFromFile() ) /* doesn't exist. browse current dir */
		mode = BROWSE;

	if ( mode == BROWSE ) // if we're browsing read the CWD
		listFromDir();

	/* main event loop */
	/* determines the entry to use */
	for ( ; ; )
	{
		displayList();

		if ( ! userInteraction(getch()) )
			break;
	}

	finish(getCurrentEntry(), true);
	exit(1);
}
