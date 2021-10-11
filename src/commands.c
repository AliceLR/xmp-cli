/* Extended Module Player
 * Copyright (C) 1996-2016 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See the COPYING
 * file for more information.
 */

#include <unistd.h>
#if defined(_WIN32) || defined(__OS2__) || defined(__DJGPP__) || defined(_DOS)
#include <conio.h>
#endif
#if defined(AMIGA) || defined(__AMIGA__) || defined(__AROS__)
#ifdef __amigaos4__
#define __USE_INLINE__
#endif
#include <proto/exec.h>
#include <proto/dos.h>
#endif
#include <xmp.h>
#include "common.h"

#ifdef __CYGWIN__
#include <sys/select.h>

/*
 * from	daniel åkerud <daniel.akerud@gmail.com>
 * date	Tue, Jul 28, 2009 at 9:59 AM
 *
 * Under Cygwin, the read() in process_echoback blocks because VTIME = 0
 * is not handled correctly. To fix this you can either:
 *
 * 1. Enable "tty emulation" in Cygwin using an environment variable.
 * http://www.mail-archive.com/cygwin@cygwin.com/msg99417.html
 * For me this is _very_ slow and I can see the characters as they are
 * typed out when running xmp. I have not investigated why this is
 * happening, but there is of course a reason why this mode is not
 * enabled by default.
 *
 * 2. Do a select() before read()ing if the platform is Cygwin.
 * This makes Cygwin builds work out of the box with no fiddling around,
 * but does impose a neglectible cpu overhead (for Cygwin builds only).
 */
static int stdin_ready_for_reading(void)
{
	fd_set fds;
	struct timeval tv;
	int ret;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);

	ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

	if (ret > 0 && FD_ISSET(STDIN_FILENO, &fds))
		return 1;

	return 0;
}
#endif

static int read_key(void)
{
	char key;
	int ret = 0;

#if defined(HAVE_TERMIOS_H) && !(defined(_WIN32) || defined(__DJGPP__) || defined(_DOS))
#ifdef __CYGWIN__
	if (stdin_ready_for_reading())
#endif
		ret = read(0, &key, 1);
#elif defined(_WIN32) || defined(__OS2__) || defined(__DJGPP__) || defined(_DOS)
	if (kbhit()) {
		key = getch();
		ret = 1;
	}
#elif defined(AMIGA) || defined(__AMIGA__) || defined(__AROS__)
	/* Amiga CLI */
	{
		BPTR in = Input();
		if (WaitForChar(in, 1)) {
			Read(in, &key, 1);
			ret = 1;
		}
	}
#else
	ret = 0;
#endif

	if (ret <= 0) {
		return -1;
	}

	return key;
}

static void change_sequence(xmp_context handle, const struct xmp_module_info *mi, struct control *ctl, int i)
{
	int seq = ctl->sequence;

	seq += i;

	if (seq >= mi->num_sequences) {
		seq = 0;
	} else if (seq < 0) {
		seq = mi->num_sequences - 1;
	}

	if (seq == ctl->sequence) {
		info_message("Sequence not changed: only one sequence available");
	} else {
		ctl->sequence = seq;
		info_message("Change to sequence %d", seq);
		xmp_set_position(handle, mi->seq_data[seq].entry_point);
	}
}

/* Interactive commands */

/* HACK */
static int keyboard_us(int cmd)
{
	switch(cmd) {
	case 'z': return 0; // c
	case 's': return 1; // c#
	case 'x': return 2; // etc
	case 'd': return 3;
	case 'c': return 4;
	case 'v': return 5;
	case 'g': return 6;
	case 'b': return 7;
	case 'h': return 8;
	case 'n': return 9;
	case 'j': return 10;
	case 'm': return 11;
	case 'q': return 12; // c + 1
	case '2': return 13;
	case 'w': return 14;
	case '3': return 15;
	case 'e': return 16;
	case 'r': return 17;
	case '5': return 18;
	case 't': return 19;
	case '6': return 20;
	case 'y': return 21;
	case '7': return 22;
	case 'u': return 23;
	case 'i': return 24; // c + 2
	case '9': return 25;
	case 'o': return 26;
	case '0': return 27;
	case 'p': return 28;
	}
	return -1;
}
/* END HACK */

/* VT100 ESC sequences:
 * ESC [ A - up arrow
 * ESC [ B - down arrow
 * ESC [ C - right arrow
 * ESC [ D - left arrow
 */
void read_command(xmp_context handle, const struct xmp_module_info *mi, struct control *ctl)
{
	/* HACK */
	static int octave = 3;
	static int instrument = 0;
	int note;
	/* END HACK */
	int cmd;

	cmd = read_key();
	if (cmd <= 0)
		return;

	/* HACK */
	switch(cmd) {
	case 'Q':		/* quit */
		xmp_stop_module(handle);
		ctl->pause = 0;
		ctl->skip = -2;
		break;
	case ',': /* decrement octave */
	case '<':
		if (octave) octave--;
		break;
	case '.': /* increment octave */
	case '>':
		if (octave < 9) octave++;
		break;
	case '[': /* previous instrument */
	case '(':
		if (instrument) instrument--;
		break;
	case ']': /* next instrument */
	case ')':
		if (instrument + 1 < mi->mod->ins) instrument++;
		break;
	case '`': /* note cut */
		xmp_smix_play_instrument(handle, instrument, XMP_KEY_CUT, 0, 0);
		break;
	case '=': /* note off */
		xmp_smix_play_instrument(handle, instrument, XMP_KEY_OFF, 0, 0);
		break;
	case '+': /* note fade */
		xmp_smix_play_instrument(handle, instrument, XMP_KEY_FADE, 0, 0);
		break;
	default: /* note */
		note = keyboard_us(cmd);
		if (note >= 0) {
			xmp_smix_play_instrument(handle, instrument, note + octave * 12, 64, 0);
		}
	}
	return;
	/* END HACK */

	switch (cmd) {
	case 0x1b:		/* escape */
		cmd = read_key();
		if (cmd != '[')
			goto cmd_quit;
		cmd = read_key();
		switch (cmd) {
		case 'A':
			goto cmd_next_mod;
		case 'B':
			goto cmd_prev_mod;
		case 'C':
			goto cmd_next_pos;
		case 'D':
			goto cmd_prev_pos;
		}

		break;
	case 'q':		/* quit */
	cmd_quit:
		xmp_stop_module(handle);
		ctl->pause = 0;
		ctl->skip = -2;
		break;
	case 'f':		/* jump to next order */
	cmd_next_pos:
		xmp_next_position(handle);
		ctl->pause = 0;
		break;
	case 'b':		/* jump to previous order */
	cmd_prev_pos:
		xmp_prev_position(handle);
		ctl->pause = 0;
		break;
	case 'n':		/* skip to next module */
	cmd_next_mod:
		xmp_stop_module(handle);
		ctl->pause = 0;
		ctl->skip = 1;
		break;
	case 'p':		/* skip to previous module */
	cmd_prev_mod:
		xmp_stop_module(handle);
		ctl->pause = 0;
		ctl->skip = -1;
		break;
	case 'l':
		ctl->loop++;
		ctl->loop %= 3;
		break;
	case 'X':
		ctl->cur_info = 'X';
		break;
	case 'a': {
		int f;

		ctl->amiga_mixer = !ctl->amiga_mixer;

		/* set player flags */
		f = xmp_get_player(handle, XMP_PLAYER_FLAGS);
		if (ctl->amiga_mixer) {
			xmp_set_player(handle, XMP_PLAYER_FLAGS,
						f | XMP_FLAGS_A500);
		} else {
			xmp_set_player(handle, XMP_PLAYER_FLAGS,
						f &= ~XMP_FLAGS_A500);
		}

		/* set current module flags */
		f = xmp_get_player(handle, XMP_PLAYER_CFLAGS);
		if (ctl->amiga_mixer) {
			xmp_set_player(handle, XMP_PLAYER_CFLAGS,
						f | XMP_FLAGS_A500);
		} else {
			xmp_set_player(handle, XMP_PLAYER_CFLAGS,
						f &= ~XMP_FLAGS_A500);
		}
		break; }
	case 'Z':
		ctl->cur_info = 'Z';
		break;
	case 'z':
		ctl->explore ^= 1;
		break;
	case ' ':		/* paused module */
		ctl->pause ^= 1;
		break;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		/* toggle mute */
		xmp_channel_mute(handle, cmd - '1', 2);
		break;
	case '0':
		xmp_channel_mute(handle, 9, 2);
		break;
	case '!': {
		int i;
		for (i = 0; i < 10; i++) {
			xmp_channel_mute(handle, i, 0);
		}
		break; }
	case '?':
	case 'c':
	case 'i':
	case 'I':
	case 'S':
	case 'm':
		ctl->display = cmd;
		break;
	case 'h':
		ctl->display = '?';
		break;
	case '>':
		change_sequence(handle, mi, ctl, 1);
		break;
	case '<':
		change_sequence(handle, mi, ctl, -1);
		break;
	}
}
