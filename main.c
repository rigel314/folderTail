/*
 * main.c
 *
 *  Created on: Aug 3, 2013
 *      Author: cody
 *  TODO: Actually get file info for title
 *  TODO: Refresh title every 'tick'
 *  TODO: tail -f
 *  TODO: Rescan for files
 *    TODO: Add and remove files, windows, etc as necessary
 */

#include <ncurses.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

struct windowlist
{
	struct windowlist* next;
	WINDOW* title;
	WINDOW* content;
	char* fullname;
	struct stat info;
};

struct windowlist* winlist;
int numSplits = 0;
struct filelist* filelist = NULL;

void mkWins(struct windowlist* ent);
void writeTitles(struct windowlist* list);
void refreshAll(struct windowlist* list);
void resizeAll(struct windowlist* list);
void findAllFiles(struct windowlist** list, char* path);
struct windowlist* winAtIndex(struct windowlist* list, int index);
void addFile(struct windowlist** list, char* name, struct stat statinfo);

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
	struct windowlist* ptr;

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

	findAllFiles(&winlist, argv[1]);
//	for(ptr = winlist; ptr != NULL; ptr = ptr->next)
//		printf("%s\t%d\n", ptr->fullname, (int) ptr->info.st_size);

//	return 0;

	initscr();
	raw();
	noecho();
	curs_set(0);
	keypad(stdscr,TRUE);
	halfdelay(3);
	refresh();

	for(ptr = winlist; ptr != NULL; ptr = ptr->next)
		mkWins(ptr);

	resizeAll(winlist);
	writeTitles(winlist);
	refreshAll(winlist);

	while((c = getch()))
	{
		switch(c)
		{
			case 10:
				endwin();
				return 0;
				break;
		}
		refreshAll(winlist);
	}

	return 0;
}

void mkWins(struct windowlist* ent)
{
	ent->title = newwin(0, 0, 0, 0);
	ent->content = subwin(ent->title, 0, 0, 0, 0);
	return;
}

//void unsplit(WINDOW* win)
//{
//	int i;
//	int flag = 0;
//
//	for(i = 0; i<numSplits; i++)
//	{
//		if(flag == 1)
//			winlist[i-1] = winlist[i];
//		if(winlist[i] == win)
//		{
//			flag = 1;
//			werase(win);
//			refreshAll(winlist, numSplits);
//			delwin(win);
//		}
//	}
//	if(!flag)
//		return;
//
//	if(!numSplits)
//		return;
//
//	winlist = realloc(winlist, sizeof(WINDOW*) * --numSplits);
//	if(!winlist)
//		return;
//
//	resizeAll(winlist, numSplits);
//	return;
//}

void writeTitles(struct windowlist* list)
{
	struct windowlist* ptr;
	int r, c, len, i;

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
		len = snprintf(NULL, 0, "%s - %s", "12KB", "2013/08/05 00:24:24");
		mvwprintw(ptr->title, r-1, c-len, "%s - %s", "12KB", "2013/08/05 00:24:24");

		// Stop highlighting
		wattroff(ptr->title, A_STANDOUT);
	}
}

void refreshAll(struct windowlist* list)
{
	struct windowlist* ptr;

	for(ptr = winlist; ptr != NULL; ptr = ptr->next)
	{
		wrefresh(ptr->title);
		touchwin(ptr->title);
		wrefresh(ptr->content);
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
		if(i<counter)
		{
			wresize(ptr->title, LINES/numWins+1, COLS);
			mvwin(ptr->title, (LINES/numWins+1) * i, 0);
			wresize(ptr->content, LINES/numWins, COLS);
		}
		else
		{
			wresize(ptr->title, LINES/numWins, COLS);
			mvwin(ptr->title, counter + (LINES/numWins) * i, 0);
			wresize(ptr->content, LINES/numWins-1, COLS);
		}
		werase(ptr->title);
//		mvwprintw(ptr->title, 1, 1, "%s", "test");
//		box(wins[i], 0, 0);
	}
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
		if(!st && S_ISREG(statbuf->st_mode))
			addFile(list, fullname, *statbuf);
	}
	closedir(dp);

	return;
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

	if(!*list)
	{
		*list = new;
		(*list)->next = NULL;
		return;
	}

	for(ptr = *list; ptr->next != NULL; ptr = ptr->next);

	ptr->next = new;
}
