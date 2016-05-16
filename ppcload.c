/*									    	*/
/* 	eEK/eOS loader for Motorola boards with DINK32/MDINK32 rom monitor.	*/
/* 	Copyright (C) 1999, Phillip Stanley-Marbell. All rights reserved. 	*/
/*          This software is provided with ABSOLUTELY NO WARRANTY. 	     	*/
/*									     	*/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>


struct termios	newsetting;


enum
{
	MAXTIMEOUT = 4096,
	MAXRESPONSELEN = 1024,
	MAXLINELEN = 1024,
	MAXFILENAMELEN = 64,
	MAXDINKCMDLEN = 64,
	ROMSTRLEN = 64,
	PORTNAMELEN = 16,
};


typedef struct
{
	char romversion[ROMSTRLEN];
	long baudrate;
	char port[PORTNAMELEN];
	char status[MAXRESPONSELEN];
	unsigned com;
} Board;


enum
{
	ERROR = -1,
	OK = 1,
};


static int	init(Board *);
static void	load(Board *);
static void	run(Board *);
static void	regdump(Board *);
static void	memdump(Board *);
static void	getprompt(Board *);
static void	command(Board *, char *);
static void	reset(Board *);


int
init(Board *board)
{
	strncpy(board->romversion, "", ROMSTRLEN);
	strncpy(board->port, "/dev/cua00", PORTNAMELEN);
	strncpy(board->status, "", MAXRESPONSELEN);

	board->baudrate = B9600 | CS8 | CREAD;
	board->com = 0;

	if ((board->com = open(board->port, O_RDWR|O_NDELAY)) == ERROR)
	{
		fprintf(stderr, "ppcload : ERROR - Could not open %s\n", board->port);
		exit(-1);
	}

	if (ioctl(board->com, TIOCGETA, &newsetting) < 0)
	{
		perror(0);
		fprintf(stderr, "ppcload : ERROR - ioctl GET failed on %s\n", board->port);
		exit(-1);
	}

	newsetting.c_iflag = 0L;
	newsetting.c_oflag = 0L;
	newsetting.c_cflag = board->baudrate;
	newsetting.c_lflag = 0L;
	newsetting.c_cc[VTIME] = 0;
	newsetting.c_cc[VMIN] = 0;

	if (ioctl(board->com, TIOCSETA, &newsetting) < 0)
	{
		perror(0);
		fprintf(stderr, "ppcload : ERROR - ioctl SET failed on %s\n", board->port);
		exit(-1);
	}

	return OK;
}


int
main(int argc, char *argv[])
{
	Board *board;
	char cmd=0, buf[16];

	system("clear");
  	printf("\n\neEK/eOS loader for Motorola boards with DINK32/MDINK32 rom monitor.\n");
  	printf("Copyright (C) 2000, Phillip Stanley-Marbell. All Rights Reserved.\n");
  	printf("This software is provided with ABSOLUTELY NO WARRANTY.\n\n");

	if ((board = (Board *) malloc(sizeof(Board))) == NULL)
	{
		fprintf(stderr, "ppcload : ERROR - no memory\n");
		exit(-1);
	}

	init(board);

	if (argc > 1) 
	{
             if (strncmp(argv[1], "9600", 4) == 0) 
	     {
	     }
             else 
	     {
	        printf("Usage: ppcload [9600 | 19200 | 38400] \n");
		exit(1);
	     }
	}
        else 
	{
	     printf("ppcload : Using %s at 9600 baud\n", board->port);
	}

  	while (cmd != 'q') 
	{
	  	printf("\nAccepts DINK32 Commands\nAdditional PPCload Commands :\n");
  		printf("q - Quit\n");
  		printf("l - Load S-Record File\n");
  		printf("g - Go !\n");
		printf("r - Read Registers\n");
		printf(". - Reset\n");
		printf("m - Dump Memory\n");
		printf("x - getprompt()\n");
  		printf("PPCload >> ");

		rewind(stdin);
  		fgets(buf, MAXDINKCMDLEN, stdin);
  		cmd = buf[0];

		switch(cmd) 
		{
			case 'q':
				break;

			case 'l':
				load(board);
				break;

			case 'g': 
				run(board);
				break;

			case 'r': 
				regdump(board);
				break;

			case '.':
				reset(board);
				break;

			case 'm':
				memdump(board);
				break;

			case 'x':
				getprompt(board);
				break;

			default :
				buf[strlen(buf)-1] = '\0';
			   	command(board, buf);
				break;
		}
  	}

	return 0;
}


void
load(Board *board)
{
	FILE *fd;
	long loadaddr = 0;
	char filename[MAXFILENAMELEN], line[MAXLINELEN];
	int timeout = 0, n = 0;
	char readch, *tptr;

	/* Note there is a magic number (MAXFILENAMELEN) below */
	printf("S-Record File: ");
	scanf("%64s", filename);
	
	fd = fopen(filename, "r");
	if (fd == NULL)
	{
		fprintf(stderr, "ppcload : ERROR - Could not open file %s.\n",filename);
		return;
	}

	/* Initiate download */
	command(board, "dl -k");

	while(fgets(line, MAXLINELEN, fd))
	{
		int lines = 0;
		

		/* Never valid for SREC, most likely last newline */
		if (strlen(line) <= 1)
		{
			break;
		}

		//fprintf(stderr, "%s", line );

		tptr = &line[0];

		while (*tptr != '\r')
		{
			timeout = 0;
			n = 0;
			do
			{
				n = write(board->com, tptr, 1);
				timeout++;
			} while (n != 1 && timeout < MAXTIMEOUT);

			if (timeout == MAXTIMEOUT)
			{
				fprintf(stderr, "\nchar %c of line [%d] load timed out.\n",\
						*tptr,  lines);
			}

			/* wait for echo */
			readch = ' ';
			timeout = 0;
			n = 0;
			do
			{
				n = read(board->com, &readch, 1);
				timeout++;
			}
			while (n != 1 && timeout < MAXTIMEOUT);

			if (timeout == MAXTIMEOUT)
			{
				fprintf(stderr, "-");
			}
			else
			{
				fprintf(stderr, "+");
			}
		
			tptr++;
		}



		/* send CR */
		timeout = 0;
		n = 0;
		do
		{
			n = write(board->com, "\n", 1);
			timeout++;
		} while (n != 1 && timeout < MAXTIMEOUT);

		if (timeout == MAXTIMEOUT)
		{
			fprintf(stderr, "\nsend newline timed out.\n");
		}

		/* wait for echo */
		readch = ' ';
		timeout = 0;
		n = 0;
		do
		{
			n = read(board->com, &readch, 1);
			timeout++;
		}
		while (n != 1 && timeout < MAXTIMEOUT);

		if (timeout == MAXTIMEOUT)
		{
			//fprintf(stderr, "\nSREC load line [%d] ECHO timed out.\n", lines);
		}

		lines++;
	}

	fclose(fd);

	/* Get all o/p from DINK till the next prompt */
	getprompt(board);

	return;
}


void
command(Board *board, char *cmd)
{
	int n, timeout = 0;
	char readch, *tptr = cmd;

	/*										*/
	/*	Keys typed to DINK are echoed, so send each char and grab its echo.	*/
	/*										*/
	
	getprompt(board);
	//fprintf(stderr, "\ncommand is <%s>\n", tptr);

	while (*tptr != '\0')
	{
		/* send key */
		timeout = 0;
		n = 0;
		do
		{
			n = write(board->com, tptr, 1);
			timeout++;
		}
		while (n != 1 && timeout < MAXTIMEOUT);

		if (timeout == MAXTIMEOUT)
		{
			fprintf(stderr, "\nkey send timed out in command()\n");
		}
		else
		{
			//fprintf(stderr, "wrote [%c]\n", *tptr);
		}

		/* wait for echo */
		readch = ' ';
		timeout = 0;
		n = 0;
		do
		{
			n = read(board->com, &readch, 1);
			timeout++;
		}
		while (readch != *tptr && timeout < MAXTIMEOUT);

		if (timeout == MAXTIMEOUT)
		{
			fprintf(stderr, "\nreceipt of key echo timed out in command()\n");
		}
		else
		{
			//fprintf(stderr, "echo [%c]\n", readch);
		}

		tptr++;
	}

	getprompt(board);
	
	return;
}

void
run(Board *board)
{
	long runaddr;
	char cmd[MAXDINKCMDLEN];

	printf("Run Address: ");
	scanf("%lx", &runaddr);
	sprintf(cmd, "go %lx", runaddr);

	command(board, cmd);

	return;
}

void
regdump(Board *board)
{
	return;
}


void
memdump(Board *board)
{
	return;
}


void
reset(Board *board)
{
	return;
}


void
getprompt(Board *board)
{
	int 	n, timeout;
	char 	readch;

	/* read any lingering o/p */
	n = 0;
	timeout = 0;
	do
	{
		n = read(board->com, &readch, 1);
		timeout++;
	}
	while (n != 1 && timeout < MAXTIMEOUT);


	/* write a newline just in case */
	n = 0;
	do
	{
		n = write(board->com, "\n", 1);
	}
	while (n != 1);

	timeout = 0;
	do
	{
		n = 0;
		do
		{
			n = read(board->com, &readch, 1);
			timeout++;
		} while (n != 1 && timeout <= MAXTIMEOUT);

		fprintf(stderr,"%c", readch);
	}
	while (readch != '>' && timeout < MAXTIMEOUT);

	if (timeout == MAXTIMEOUT)
	{
		fprintf(stderr, "ppcload : Timed out on getting first \">\"\n");

		return;
	}

	/* Read the second '>' */
	readch = ' ';
	timeout = 0;
	do 
	{
		read(board->com, &readch, 1);
		timeout++;
	}
	while (readch != '>' && timeout < MAXTIMEOUT);

	if (timeout == MAXTIMEOUT)
	{
		fprintf(stderr, "PPCload : Timed out on getting second \">\"\n");

		return;
	}

	fprintf(stderr,"%c", readch);

	return;
}
