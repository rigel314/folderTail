/*
 * main.c
 *
 *  Created on: Aug 3, 2013
 *      Author: cody
 *  TODO: tail -f
 *  TODO: reap processes & close file descriptors
 */

#define min(x, y) ((x)<(y)?(x):(y))
#define max(x, y) ((x)>(y)?(x):(y))

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
#include <signal.h>
#include <sys/select.h>

// typedef enum { false=0, true=1 } bool;

struct windowlist
{
	struct windowlist* next;
	WINDOW* title;
	WINDOW* content;
	char* fullname;
	struct stat info;
	pid_t pid;
	int fd;
	int wfd;
	int err;
};

struct windowlist* winlist;
int numSplits = 0;
struct filelist* filelist = NULL;

void help();
void mkWins(struct windowlist* ent);
char* getSizeStr(int size);
void writeTitles(struct windowlist* list);
void writeContents(struct windowlist* list);
void addContents(struct windowlist* ent, char* buffer, int nBytes);
void writeAllRefresh(struct windowlist* list);
void refreshAll(struct windowlist* list);
void resizeAll(struct windowlist* list);
void rescanFiles(struct windowlist** list, char* path);
bool fileExistsInList(struct windowlist* list, char* name);
void findAllFiles(struct windowlist** list, char* path);
struct windowlist* winAtIndex(struct windowlist* list, int index);
void addFile(struct windowlist** list, char* name, struct stat statinfo);
void freeFile(struct windowlist** list, struct windowlist* win);
pid_t startTail(struct windowlist* ptr);
void cleanup(struct windowlist* list);

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
	int fdMax;
	DIR* dp;
	fd_set set;
	struct timeval tv;
	struct windowlist* ptr;

	// I only take one argument.
	if(argc != 2)
	{
		help();
		return 1;
	}

	// Check that the directory is accessible.
	dp = opendir(argv[1]);
	if(!dp)
	{
		printf("Error opening directory: %s\nopendir returns: %d\n", argv[1], errno);
		return 2;
	}
	closedir(dp);

	/* DEBUG */
//	findAllFiles(&winlist, argv[1]);
//	for(ptr = winlist; ptr != NULL; ptr = ptr->next)
//		printf("%s\t%s\n", ptr->fullname, getSizeStr((int) ptr->info.st_size));
//
//	return 0;

	// ncurses init stuff
	initscr();
	raw();
	noecho();
	curs_set(0);
	keypad(stdscr,TRUE);
	halfdelay(3);
	refresh();

	// Find new files, fork() child processes to start tailing them.  Write the title bars(and technically the blank tail outputs)
	rescanFiles(&winlist, argv[1]);
	writeAllRefresh(winlist);

	// Main loop for keypresses and files to be read.
	while(1)
	{
		// Maximum file descriptor.  Needed for select().  Start at zero to ensure it will be changed to a bigger one.
		fdMax = 0;

		// Wait, at maximum, one second for new input, then check for new or deleted files.
		tv.tv_sec=1;
		tv.tv_usec=0;

		// Zero out the set, then add 0 to the set to monitor stdin for keypresses.
		FD_ZERO(&set);
		FD_SET(0, &set);

		// Add each file descriptor to the set and keep track of the highest file descriptor.
		for(ptr = winlist; ptr != NULL; ptr = ptr->next)
		{
			fdMax = max(fdMax, ptr->fd);
			FD_SET(ptr->fd, &set);
		}

		// Monitor all fds for readability.
		if(select(fdMax+1, &set, NULL, NULL, &tv) == -1)
			if (errno != EINTR && errno != EAGAIN)
			{
				cleanup(winlist);
				endwin();
				return 4;
			}

		// A key was pressed.
		if(FD_ISSET(0, &set))
		{
			c = getch();
			switch(c)
			{
				case 'Q':
				case 'q':
				case 10:
					cleanup(winlist);
					endwin();
					return 0;
					break;
			}
		}

		// Check if any others are readable.
		for(ptr = winlist; ptr != NULL; ptr = ptr->next)
		{
			char* buffer;
			int maxBytes = 65536;
			int nBytes;

			if(FD_ISSET(ptr->fd, &set))
			{
				buffer = malloc(maxBytes);

				nBytes = read(ptr->fd, buffer, maxBytes);

				if (nBytes != -1)
					addContents(ptr, buffer, nBytes);

				free(buffer);
			}
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
	char fmt[] = "fd=%d pid=%d %s - %d/%.2d/%.2d %.2d:%.2d:%.2d";

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
		len = snprintf(NULL, 0, fmt, ptr->fd, ptr->pid, size, time.tm_year+1900, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
		mvwprintw(ptr->title, r-1, c-len, fmt, ptr->fd, ptr->pid, size, time.tm_year+1900, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
		free(size);

		// Stop highlighting
		wattroff(ptr->title, A_STANDOUT);
	}
}

void writeContents(struct windowlist* list)
{
	struct windowlist* ptr;

	for(ptr = list; ptr != NULL; ptr = ptr->next)
	{
		if(ptr->err == -1)
			mvwprintw(ptr->content, 0, 0, "ERROR: %d: %s ", ptr->err, strerror(ptr->err));
	}
}

void addContents(struct windowlist* ent, char* buffer, int nBytes)
{
	int i;

	for(i = 0; i<nBytes; i++)
	{
		waddch(ent->content, buffer[i]);
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
	int fds[2];

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

	new->pid = -1;
	new->fd = -1;
	new->wfd = -1;

	if(!*list)
	{
		*list = new;
	}
	else
	{
		for(ptr = *list; ptr->next != NULL; ptr = ptr->next);

		ptr->next = new;
	}

	if(pipe(fds) == -1)
	{
		new->err = errno;
		return;
	}

	new->fd = fds[0];
	new->wfd = fds[1];
	new->pid = startTail(new);
	if(new->pid == -1)
	{
		new->err = errno;
		return;
	}
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
			kill(ptr->pid, SIGKILL);
			close(ptr->fd);
			close(ptr->wfd);

			if(last)
				last->next = ptr->next;
			else
				*list = ptr->next;

			free(ptr);
			return;
		}
	}
}

pid_t startTail(struct windowlist* ptr)
{
	pid_t child;

	child = fork();
	if(!child)
	{
		close(1);
		dup(ptr->wfd);
		execlp("tail", "tail", "-f", ptr->fullname, (char*) NULL);
		exit(3);
	}

	return child;
}

void cleanup(struct windowlist* list)
{
	struct windowlist* ptr;

	for(ptr=list; ptr!=NULL; ptr=ptr->next)
	{
		if(ptr->pid != -1)
			kill(ptr->pid, SIGKILL);
		if(ptr->fd != -1)
			close(ptr->fd);
		if(ptr->wfd != -1)
			close(ptr->wfd);
	}
}
