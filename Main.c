////////////////////////////////////////////////////////////////////////////////
// -------------------------------------------------------------------------- //
//                                                                            //
//                          Copyright (C) 2012-2013                           //
//                            github.com/dkrutsko                             //
//                            github.com/Harrold                              //
//                            github.com/AbsMechanik                          //
//                                                                            //
//                        See LICENSE.md for copyright                        //
//                                                                            //
// -------------------------------------------------------------------------- //
////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------//
// Prefaces                                                                   //
//----------------------------------------------------------------------------//

#include "NDP.h"

#include <time.h>
#include <stdlib.h>
#include <curses.h>
#include <unistd.h>



//----------------------------------------------------------------------------//
// Locals                                                                     //
//----------------------------------------------------------------------------//

static int gX = 0; // Terminal X position of center
static int gY = 0; // Terminal Y position of center

// Color identifiers
enum
{
	NORMAL = 1,	// White on Black
	INVERTED,	// Black on White
	ERROR,		// Red on Black
	WARNING,	// Yellow on Black
};



//----------------------------------------------------------------------------//
// Menu                                                                       //
//----------------------------------------------------------------------------//

////////////////////////////////////////////////////////////////////////////////
/// <summary> Clears the terminal window and updates its size. </summary>

static void Clear (void)
{
	clear();
	getmaxyx (stdscr, gY, gX);

	gX = (int) gX * 0.5;
	gY = (int) gY * 0.5;
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Prints the application icon to the screen. </summary>

static void PrintIcon (int x, int y, bool selected)
{
	static const char sIcon[9][23] =
	{
		{"                      "},
		{"         ++++         "},
		{"        ++++++        "},
		{"       +++  +++       "},
		{"      +++    +++      "},
		{"     +++      +++     "},
		{"                      "},
		{"      Metropolis      "},
		{"                      "},
	};

	int xx, yy;
	for (yy = 0; yy < 9; ++yy)
	{
		move (y + yy, x);
		for (xx = 0; xx < 22; ++xx)
		{
			char icon = sIcon[yy][xx];

			if (icon == '+')
			{
				icon = ' ';
				if (selected)
					 attron (COLOR_PAIR (NORMAL  ));
				else attron (COLOR_PAIR (INVERTED));
			}

			else
			{
				if (selected)
					 attron (COLOR_PAIR (INVERTED));
				else attron (COLOR_PAIR (NORMAL  ));
			}

			printw ("%c", icon);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Starts the main NDP protocol loop. </summary>

static void StartNDP (void)
{
	NDP_State state;

	// Get the name of the interface to use
	mvprintw (gY+2, gX-25, "Enter the name of the wireless interface you would ");
	mvprintw (gY+3, gX-25, "like to use or leave blank to use the default (ra0)");

	refresh(); move (gY+5, gX-4);
	echo(); getnstr (state.Interface, 8); noecho();

	// Copy the specified interface name
	if (state.Interface[0] == '\0')
	{
		// Use the default interface name
		state.Interface[0] =  'r';
		state.Interface[1] =  'a';
		state.Interface[2] =  '0';
		state.Interface[3] = '\0';
	}

	// Start the Neighbor Discovery Protocol
	NDP_Create (&state);
	NDP_Start  (&state);

	Clear();
	timeout (0);

	int i, j;
	char pressed = 0;
	NDP_Neighbor* n;
	char result[80];
	long slp = 0;

	while (1)
	{
		// Check for errors
		if (state.Error == NDP_ERROR_NONE)
			mvprintw (2, gX-14, "Press Q to Return to the Menu");

		else
		{
			// Convert error index into a string
			const char* error = NDP_ErrorString (&state);

			// Center the error message
			for (i = 0; error[i] != 0; ++i);
			i = (int) i * 0.5;

			// Print the error message
			attron (COLOR_PAIR (ERROR));
			mvprintw (2, gX-i, error);
			break;
		}

		// Check for terminate key
		pressed = getch() | 32;
		if (pressed == 'q')
			break;

		if (pressed == 'f')
			state.Stress = state.Stress == 0 ? 1 : 0;

		if (state.Stress == 1)
		{
			attron (COLOR_PAIR (WARNING));
			mvprintw (3, gX-9, "--STRESS TESTING--");
			attron (COLOR_PAIR (NORMAL ));
		}

		// Sleep for 100 ms
		refresh();
		usleep (slp);
		slp = 100000;
		Clear();

		// Print interface information
		sprintf (result, "INTERFACE: %-8s INDEX: %-2d MTU: %-5d ADDRESS: %s",
				state.Interface, state.IfIndex, state.MTU, NDP_AddrString (&state.Addr));

		// Center the interface message
		for (i = 0; result[i] != 0; ++i);
		i = (int) i * 0.5;

		// Print the interface message
		mvprintw (5, gX-i, result);

		// Print NDP table
		mvprintw (7, gX-30, "      ADDRESS      |      ARRIVED      |      RECORDED      ");
		mvprintw (8, gX-30, "------------------------------------------------------------");

		NDP_Lock (&state);

		for (i = 0, j = 8; i < NDP_TABLE_LEN; ++i)
		{
			n = state.Table[i];
			if (n != NULL) mvprintw (++j, gX-30, " %-17s |       %-11s |     %6d",
				NDP_AddrString (&n->Addr), n->Arrived == 0 ? "FALSE" : "TRUE", (int) n->Recorded);
		}

		NDP_Unlock (&state);
	}

	NDP_Stop    (&state);
	NDP_Destroy (&state);

	attron (COLOR_PAIR (NORMAL));
	mvprintw (3, gX-12, "Press Any Key to Continue");
	refresh(); timeout (-1); getch();
}

////////////////////////////////////////////////////////////////////////////////
/// <summary> Starts the application menu loop. </summary>

static void StartMenu (void)
{
	timeout (-1);

	// Variables
	int option  = 0;
	int pressed = 0;

	// Create colors
	init_pair (NORMAL,   COLOR_WHITE,  COLOR_BLACK);
	init_pair (INVERTED, COLOR_BLACK,  COLOR_WHITE);
	init_pair (ERROR,    COLOR_RED,    COLOR_BLACK);
	init_pair (WARNING,  COLOR_YELLOW, COLOR_BLACK);

	// Menu loop
	while (1)
	{
		Clear();

		// Print menu options
		PrintIcon (gX-11, gY-8, option == 0);

		if (option == 1)
			 attron (COLOR_PAIR (INVERTED));
		else attron (COLOR_PAIR (NORMAL  ));

		mvprintw (gY+2, gX-11, "                      ");
		mvprintw (gY+3, gX-11, "      Developers      ");
		mvprintw (gY+4, gX-11, "                      ");

		refresh();
		pressed = getch();

		// Process user input
		if (pressed == KEY_UP ||
			pressed == KEY_LEFT)
			option = 0;

		if (pressed == KEY_DOWN ||
			pressed == KEY_RIGHT)
			option = 1;

		if (pressed == '\n')
		{
			// Start NDP
			if (option == 0)
				StartNDP();

			else
			{
				// Display author information
				attron (COLOR_PAIR (NORMAL));

				mvprintw (gY+1, gX-17, "     Copyright (C) 2012-2013     ");
				mvprintw (gY+2, gX-17, "                                 ");
				mvprintw (gY+3, gX-17, "       github.com/dkrutsko       ");
				mvprintw (gY+4, gX-17, "       github.com/Harrold        ");
				mvprintw (gY+5, gX-17, "       github.com/AbsMechanik    ");

				refresh(); getch();
			}
		}
	}
}



//----------------------------------------------------------------------------//
// Main                                                                       //
//----------------------------------------------------------------------------//

////////////////////////////////////////////////////////////////////////////////
/// <summary> Main execution point for this application. </summary>
/// <returns> Zero for success, error code for failure. </returns>

int main (void)
{
	srand (time (NULL));	// Seed randomizer

	initscr();				// Init nCurses
	start_color();			// Enable color

	noecho();				// Don't output user input
	keypad (stdscr, TRUE);	// Enable getch with keypad
	curs_set (0);			// Hide the blinking cursor

	StartMenu();			// Start the app

	endwin();				// Close nCurses
	return 0;				// Exit application
}
