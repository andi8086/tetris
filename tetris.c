#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/select.h>
#include <time.h>
#include <termios.h>

// stuff to setup terminal and have getch() and kbhit() like in DOS... always the same...
struct termios orig_termios;
void reset_terminal_mode() { tcsetattr(0, TCSANOW, &orig_termios); }
void set_conio_terminal_mode() { struct termios nt; tcgetattr(0, &orig_termios); memcpy(&nt, &orig_termios, sizeof(nt)); cfmakeraw(&nt); tcsetattr(0, TCSANOW, &nt); }
int kbhit() { struct timeval tv = { 0L, 0L }; fd_set fds; FD_ZERO(&fds); FD_SET(0, &fds); return select(1, &fds, NULL, NULL, &tv); }
int getch() { int r; unsigned char c; if ((r = read(0, &c, sizeof(c))) < 0) return r; else return c; }

#define NUM_PARTS 5
#define WIDTH 20
#define HEIGHT 24

const char *rot_table = "dlur";
const char *parts[NUM_PARTS] = {" 003", " 001", " 0000", " 0321", " 3"};
int line = 1;
int col = 1;
int current_part = 0;
int current_line = 1;
int current_col = WIDTH >> 1;
char buffer[WIDTH * HEIGHT];
char current_color = 0x10;
int rotation = 0;
uint64_t wait_time = 500000000UL;
int blocks, rows;

void color(char c) { printf("\x1B[3%d;4%dm", c & 0xF, c >> 4); }
void gotoyx(int row, int col) { printf("\x1B[%d;%dH", row, col); }
void print_block(void) { color(current_color); printf("  "); }
int randnum(int max) { return (int) ((double) max * rand() / (RAND_MAX + 1.0)); }

void move(char d)
{
	switch(d) {
	case 'u': gotoyx(--line + 1, col*2 + 1); break;
	case 'd': gotoyx(++line + 1, col*2 + 1); break;
	case 'l': gotoyx(line + 1, --col*2 + 1); break;
	case 'r': gotoyx(line + 1, ++col*2 + 1); break;
	}
}

void draw_field(void)
{
	for (int i = 0; i < HEIGHT; i++)
		for (int j = 0; j < WIDTH; j++) {
			color(buffer[i*WIDTH+j]);
			gotoyx(i+1,j*2+1);
			printf("  ");
		}
}

bool draw_or_check_part(int what)
{
	const char *s;
recheck:
	s  = parts[current_part];
	line = current_line;
	col = current_col;
	while(*s) {
		move(rot_table[(*s - '0' + rotation) & 3]);
		if (col < 0) { current_col++; goto recheck; }
		if (line < 1) { current_line++; goto recheck; }
		if (col > WIDTH-1) { current_col--; goto recheck; }
		if (line > HEIGHT-1) { current_line--; goto recheck; }
		switch(what) {
		case 1:
			if (buffer[line*WIDTH + col]) return false;
			if (line >= HEIGHT-1) return false;
			break;
		case 2:
			buffer[line*WIDTH + col] = current_color;
			break;
		default:
			print_block();
			break;
		}
		s++;
	}
	return true;
}

void game_over()
{
	gotoyx(HEIGHT/2, WIDTH/2);
	color(0x71);
	printf("Game Over!"); fflush(stdout);
	while(!kbhit());
	exit(EXIT_SUCCESS);
}

void new_part(void)
{
	current_line = 1;
	current_col = WIDTH >> 1;
	current_part = randnum(NUM_PARTS);
	current_color = current_part + 1 << 4;
	blocks++;
}

void remove_complete_lines()
{
	int i, j;
	for (i = HEIGHT-1; i > 0; i--)
	{
		for (j = 0; j < WIDTH; j++) {
			if (!buffer[i*WIDTH + j]) goto next_line;
		}
		for (j = i; j > 0; j--) {
			memcpy(&buffer[j*WIDTH], &buffer[(j-1)*WIDTH], WIDTH);
		}
		rows++;
		wait_time *= 0.8;
next_line:
		j = j;
	}
}

void fall_part()
{
	current_line++;
	if (!draw_or_check_part(1)) {
		if (current_line <= 2) game_over();
		current_line--;
		draw_or_check_part(2);
		new_part();
		draw_field();
	} else
		draw_or_check_part(0);
}

void restore(void)
{
	reset_terminal_mode();
	color(0x07);
	fputs("\x1b[?25h", stdout);
	fputs("\x1b[2J", stdout);
}

uint64_t nanosecs(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec * 1000000000UL + now.tv_nsec;
}

void print_score(void)
{
	gotoyx(1, 1); color(0x07); printf("blocks: %d, rows: %d", blocks, rows);
}

int main(void)
{
	set_conio_terminal_mode();
	fputs("\x1b[?25l\x1b[30;100m\x1b[2J", stdout);
	atexit(restore);

	current_part = randnum(3);
	memset(buffer, 0, sizeof(buffer));

	uint64_t now = nanosecs();

	while(current_line < HEIGHT) {
		remove_complete_lines();
reupdate:
		draw_field();
		fall_part();
		print_score();
		fflush(stdout);
		do if (kbhit()) {
			switch(getch()) {
			case 'w': rotation = (rotation+1) & 3; break;
			case 'a': current_col--; if (current_col < 0) current_col = 1; break;
			case 'd': current_col++; if (current_col > WIDTH-1) current_col = WIDTH-1; break;
			case 'q': return EXIT_SUCCESS;
			case 's': goto skip_delay;
			}
			current_line--;
			goto reupdate;
		}
		while(nanosecs() - now < wait_time);
skip_delay:
		now = nanosecs();
	}

	return EXIT_SUCCESS;
}
