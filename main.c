/*
 * main.c
 *
 *  Created on: Aug 3, 2013
 *      Author: cody
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

struct filelist
{
	struct filelist* next;
	char* name;
};
struct windowlist
{
	struct windowlist* next;
	WINDOW* title;
	WINDOW* content;
	char* fullname;
	struct stat info;
};

WINDOW** winlist;
int numSplits = 0;
struct filelist* filelist = NULL;

void split();
void unsplit(WINDOW* win);
void refreshAll(WINDOW** winlist, int numWins);
void resizeAll(WINDOW** winlist, int numWins);
void findAllFiles(struct filelist** list, char* path);
struct filelist* fileAtIndex(struct filelist* list, int index);
void addFile(struct filelist** list, char* name);

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
	struct filelist* ptr;

	if(argc != 2)
	{
		help();
		return 2;
	}

	dp = opendir(argv[1]);
	if(!dp)
	{
		printf("Error opening directory: %s\nopendir returns: %d\n", argv[1], errno);
		return 3;
	}
	closedir(dp);

	findAllFiles(&filelist, argv[1]);
//	for(ptr = filelist; ptr != NULL; ptr = ptr->next)
//		printf("%s\n", ptr->name);

//	return 0;

	initscr();
	raw();
	noecho();
	curs_set(0);
	keypad(stdscr,TRUE);
	refresh();

	for(ptr = filelist; ptr != NULL; ptr = ptr->next)
		split();
	refreshAll(winlist, numSplits);

	while((c = getch()))
	{
		switch(c)
		{
		case KEY_DOWN:
			if(winlist)
				unsplit(winlist[numSplits-1]);
			break;
		case KEY_UP:
			split();
			break;
		case 10:
			endwin();
			return 0;
			break;
		}
		refreshAll(winlist, numSplits);
	}

	return 0;
}

void split()
{
	winlist = realloc(winlist, sizeof(WINDOW*) * ++numSplits);
	if(!winlist)
		return;

	winlist[numSplits-1] = newwin(0,0,0,0);
	resizeAll(winlist, numSplits);
}

void unsplit(WINDOW* win)
{
	int i;
	int flag = 0;

	for(i = 0; i<numSplits; i++)
	{
		if(flag == 1)
			winlist[i-1] = winlist[i];
		if(winlist[i] == win)
		{
			flag = 1;
			werase(win);
			refreshAll(winlist, numSplits);
			delwin(win);
		}
	}
	if(!flag)
		return;

	if(!numSplits)
		return;

	winlist = realloc(winlist, sizeof(WINDOW*) * --numSplits);
	if(!winlist)
		return;

	resizeAll(winlist, numSplits);
	return;
}

void refreshAll(WINDOW** wins, int numWins)
{
	int i;

	for(i = 0; i < numWins; i++)
	{
		wrefresh(wins[i]);
	}
}

void resizeAll(WINDOW** wins, int numWins)
{
	if(!wins || !numWins)
		return;

	int i;
	int counter = LINES%numWins;

	for(i = 0; i < numWins; i++)
	{
		if(i<counter)
		{
			wresize(wins[i], LINES/numWins+1, COLS);
			mvwin(wins[i], (LINES/numWins+1) * i, 0);
		}
		else
		{
			wresize(wins[i], LINES/numWins, COLS);
			mvwin(wins[i], counter + (LINES/numWins) * i, 0);
		}
		werase(wins[i]);
		mvwprintw(wins[i], 1, 1, "%s", fileAtIndex(filelist, i)->name);
		box(wins[i], 0, 0);
	}
}

void findAllFiles(struct filelist** list, char* path)
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
			addFile(list, fullname);
	}
	closedir(dp);

	return;
}

struct filelist* fileAtIndex(struct filelist* list, int index)
{
	int i;

	for(i = 0; i<index && list->next!=NULL; list=list->next, i++);
	return list;
}

void addFile(struct filelist** list, char* name)
{
	struct filelist* ptr;

	if(!list)
	{
		printf("died");
		return;
	}

	struct filelist* new = malloc(sizeof(struct filelist));
	if(!new)
	{
		printf("died");
		return;
	}

	new->name = strdup(name);
	if(!new->name)
	{
		printf("died");
		return;
	}

	if(!*list)
	{
		*list = new;
		(*list)->next = NULL;
		return;
	}

	for(ptr = *list; ptr->next != NULL; ptr = ptr->next);

	ptr->next = new;
}
