/*
 * main.c
 *
 *  Created on: Aug 3, 2013
 *      Author: cody
 *  TODO: tail -f
 *  TODO: reap processes & close file descriptors
 */

typedef enum { false=0, true=1 } bool;

#include <ncurses.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>

struct windowlist
{
	struct windowlist* next;
	WINDOW* title;
	WINDOW* content;
	char* fullname;
	struct stat info;
	pid_t pid;
	int fd;
};

struct windowlist* winlist;
int numSplits = 0;
struct filelist* filelist = NULL;

void help();
void mkWins(struct windowlist* ent);
char* getSizeStr(int size);
void writeTitles(struct windowlist* list);
void writeContents(struct windowlist* list);
void writeAllRefresh(struct windowlist* list);
void refreshAll(struct windowlist* list);
void resizeAll(struct windowlist* list);
void rescanFiles(struct windowlist** list, char* path);
bool fileExistsInList(struct windowlist* list, char* name);
void findAllFiles(struct windowlist** list, char* path);
struct windowlist* winAtIndex(struct windowlist* list, int index);
void addFile(struct windowlist** list, char* name, struct stat statinfo);
void freeFile(struct windowlist** list, struct windowlist* win);

void help()
{
	printf(
		"folderTail 0.1 ( http://computingeureka.com )\n"
		"Usage: folderTail <path-to-folder>\n\n"
		"Starts tail -f on each file or link that is a direct child of path-to-folder.\n"
	);
}

int main(int argc, char** argv)
{
	int c;
	DIR* dp;
//	struct windowlist* ptr;

	if(argc != 2)
	{
		help();
		return 1;
	}

	dp = opendir(argv[1]);
	if(!dp)
	{
		printf("Error opening directory: %s\nopendir returns: %d\n", argv[1], errno);
		return 2;
	}
	closedir(dp);

//	findAllFiles(&winlist, argv[1]);
//	for(ptr = winlist; ptr != NULL; ptr = ptr->next)
//		printf("%s\t%s\n", ptr->fullname, getSizeStr((int) ptr->info.st_size));
//
//	return 0;

	initscr();
	raw();
	noecho();
	curs_set(0);
	keypad(stdscr,TRUE);
	halfdelay(3);
	refresh();

	rescanFiles(&winlist, argv[1]);
	writeAllRefresh(winlist);

	while((c = getch()))
	{
		switch(c)
		{
			case 'Q':
			case 'q':
			case 10:
				endwin();
				return 0;
				break;
		}
		rescanFiles(&winlist, argv[1]);
		writeAllRefresh(winlist);
	}

	return 0;
}

void mkWins(struct windowlist* ent)
{
	ent->title = newwin(0, 0, 0, 0);
	ent->content = subwin(ent->title, 0, 0, 0, 0);
	scrollok(ent->content, true);
	idlok(ent->content, true);
	return;
}

char* getSizeStr(int size)
{
	char* cat;
	char* out;
	int i, len;
	int s = size;
	for(i = 9; !(s/(int)pow(10, i)) && i; i-=3); // Maybe 'i' will start at 24 someday.
	switch(i)
	{
		case 24:
			cat = "YB";
			break;
		case 21:
			cat = "ZB";
			break;
		case 18:
			cat = "EB";
			break;
		case 15:
			cat = "PB";
			break;
		case 12:
			cat = "TB";
			break;
		case 9:
			cat = "GB";
			break;
		case 6:
			cat = "MB";
			break;
		case 3:
			cat = "KB";
			break;
		case 0:
			cat = "B";
			break;
	}

	len = snprintf(NULL, 0, "%d%s", s/(int)pow(10, i), cat);
	out = malloc(len);
	if (!out)
		return NULL;
	sprintf(out, "%d%s", s/(int)pow(10, i), cat);
	return out;
}

void writeTitles(struct windowlist* list)
{
	struct windowlist* ptr;
	struct tm time;
	int r, c, len, i;
	char* size;

	for(ptr = list; ptr != NULL; ptr = ptr->next)
	{
		getmaxyx(ptr->title, r, c);

		// Make a line.
		wmove(ptr->title, r-1, 0);
		wattron(ptr->title, A_STANDOUT);
		for(i = 0; i < c; i++)
			waddch(ptr->title, ' ');

		// Left aligned file name
		mvwprintw(ptr->title, r-1, 0, "%s", ptr->fullname);

		// Right aligned file info
		size = getSizeStr((int) ptr->info.st_size);
		if(!size)
			continue;
		localtime_r(&ptr->info.st_mtime, &time);
		len = snprintf(NULL, 0, "%s - %d/%.2d/%.2d %.2d:%.2d:%.2d", size, time.tm_year+1900, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
		mvwprintw(ptr->title, r-1, c-len, "%s - %d/%.2d/%.2d %.2d:%.2d:%.2d", size, time.tm_year+1900, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
		free(size);

		// Stop highlighting
		wattroff(ptr->title, A_STANDOUT);
	}
}

void writeContents(struct windowlist* list)
{
	struct windowlist* ptr;
	static int c = 0;

	for(ptr = list; ptr != NULL; ptr = ptr->next)
	{
		wprintw(ptr->content, "test%d: %p\naoeu\n", c++, ptr);
	}
}

void writeAllRefresh(struct windowlist* list)
{
	resizeAll(list);
	writeContents(list);
	writeTitles(list);
	refreshAll(list);
	return;
}

void refreshAll(struct windowlist* list)
{
	struct windowlist* ptr;

	for(ptr = winlist; ptr != NULL; ptr = ptr->next)
	{
		touchwin(ptr->content);
		wrefresh(ptr->title);
	}
}

void resizeAll(struct windowlist* list)
{
	if(!list)
		return;

	int i;
	int counter;
	int numWins;
	struct windowlist* ptr;

	for(numWins = 0, ptr=list; ptr!=NULL; ptr=ptr->next, numWins++);

	counter = LINES%numWins;

	for(i = 0, ptr=list; i < numWins; ptr=ptr->next, i++)
	{
		wmove(ptr->title, getmaxy(ptr->title)-1, 0);
		wclrtoeol(ptr->title);

		if(i<counter)
		{
			wresize(ptr->title, LINES/numWins+1, COLS);
			mvwin(ptr->title, (LINES/numWins+1) * i, 0);
			wresize(ptr->content, LINES/numWins, COLS);
//			mvwin(ptr->content, (LINES/numWins+1) * i, 0);
		}
		else
		{
			wresize(ptr->title, LINES/numWins, COLS);
			mvwin(ptr->title, counter + (LINES/numWins) * i, 0);
			wresize(ptr->content, LINES/numWins-1, COLS);
//			mvwin(ptr->content, counter + (LINES/numWins) * i, 0);
		}
//		werase(ptr->title);
//		mvwprintw(ptr->title, 1, 1, "%s", "test");
//		box(wins[i], 0, 0);
	}
}

void rescanFiles(struct windowlist** list, char* path)
{
	struct windowlist* ptr;
	struct windowlist* last;
	int st;

	if(!list)
		return;
	if(!*list)
	{ // If there aren't any files yet.
		findAllFiles(list, path);
		return;
	}

	last = (*list)->next;

	// Delete windows for files that don't exist. And update the stat info for each.
	for(ptr=*list; ptr!=NULL; ptr=ptr->next)
	{
		st = stat(ptr->fullname, &ptr->info);
		if(st || !S_ISREG(ptr->info.st_mode))
		{
			freeFile(list, ptr);
			ptr = last;
		}
		last = ptr;
	}

	// Get new windows.
	findAllFiles(list, path);
}

bool fileExistsInList(struct windowlist* list, char* name)
{
	struct windowlist* ptr;

	for(ptr=list; ptr!=NULL; ptr=ptr->next)
	{
		if(!strcmp(ptr->fullname, name))
			return true;
	}
	return false;
}

void findAllFiles(struct windowlist** list, char* path)
{
	DIR* dp;
	int st;
	char* fullname;
	struct stat* statbuf;
	struct dirent* dir;

	fullname = strdup(path);
	if(!fullname)
		return;

	statbuf = malloc(sizeof(struct stat));
	if(!statbuf)
		return;

	dp = opendir(path);
	while ((dir = readdir(dp)) != NULL)
	{
		fullname = realloc(fullname, sizeof(char) * (strlen(path) + strlen(dir->d_name) + 2));
		if(!fullname)
			return;
		sprintf(fullname+strlen(path), "/%s", dir->d_name);
		st = stat(fullname, statbuf);
		if(!st && S_ISREG(statbuf->st_mode) && !fileExistsInList(*list, fullname))
			addFile(list, fullname, *statbuf);
	}
	closedir(dp);

	free(statbuf);
}

int winCount(struct windowlist* list, int index)
{
	int numWins;
	struct windowlist* ptr;
	for(numWins = 0, ptr=list; ptr!=NULL; ptr=ptr->next, numWins++);
	return numWins;
}

struct windowlist* winAtIndex(struct windowlist* list, int index)
{
	int i;

	for(i = 0; i<index && list->next!=NULL; list=list->next, i++);
	return list;
}

void addFile(struct windowlist** list, char* name, struct stat statinfo)
{
	struct windowlist* ptr;

	if(!list)
		return;

	struct windowlist* new = malloc(sizeof(struct windowlist));
	if(!new)
		return;

	new->fullname = strdup(name);
	if(!new->fullname)
		return;

	new->info = statinfo;
	new->next = NULL;
	mkWins(new);

	if(!*list)
	{
		*list = new;
		return;
	}

	for(ptr = *list; ptr->next != NULL; ptr = ptr->next);

	ptr->next = new;
}

void freeFile(struct windowlist** list, struct windowlist* win)
{
	struct windowlist* ptr;
	struct windowlist* last = NULL;

	for(ptr=*list; ptr!=NULL; ptr=ptr->next)
	{
		if(ptr->next == win)
			last = ptr;

		if(ptr == win)
		{
			delwin(ptr->content);
			delwin(ptr->title);
			free(ptr->fullname);
			if(last)
				last->next = ptr->next;
			else
				*list = ptr->next;
			free(ptr);
			return;
		}
	}
}
