/*
 * main.c
 *
 *  Created on: Aug 3, 2013
 *      Author: cody
 */

#include <ncurses.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

WINDOW** winlist;
int numSplits = 0;

void split();
void unsplit(WINDOW* win);
void refreshAll(WINDOW** winlist, int numWins);
void resizeAll(WINDOW** winlist, int numWins);

int main(int argc, char** argv)
{
	int c;

//	FILE* fp;
//	fp = fopen(argv[1],"rb");
//	if(!fp)
//	{
//		printf("Error opening file: %s\nfopen returns: %d\n",argv[1],errno);
//		return 1;
//	}
//	fclose(fp);

	initscr();
	raw();
	noecho();
	curs_set(0);
	keypad(stdscr,TRUE);

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
		mvwprintw(wins[i], 1, 1, "LINES: %d\tCOLS: %d\tcounter: %d\ti: %d", LINES, COLS, counter, i);
		box(wins[i], 0, 0);
	}
}
