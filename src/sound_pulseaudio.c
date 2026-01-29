/* Extended Module Player
 * Copyright (C) 1996-2026 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See the COPYING
 * file for more information.
 */

#include <unistd.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include "sound.h"

static pa_simple *s;
static int downmix_24;


static int init(struct options *options)
{
	pa_sample_spec ss;
	int format = -1;
	int error;

	downmix_24 = 0;
	if ((options->format & XMP_FORMAT_32BIT) && options->format_downmix == 24) {
#ifdef PA_SAMPLE_S24_32NE
		format = PA_SAMPLE_S24_32NE;
		downmix_24 = 1;
#else
		options->format_downmix = 0;
#endif
	}

	if (format < 0 && (options->format & XMP_FORMAT_32BIT)) {
#ifdef PA_SAMPLE_S32NE
		format = PA_SAMPLE_S32NE;
#else
		options->format &= ~XMP_FORMAT_32BIT;
#endif
	}

	if (format < 0 && (options->format & XMP_FORMAT_8BIT)) {
		format = PA_SAMPLE_U8;
		options->format |= XMP_FORMAT_UNSIGNED;
	} else {
		options->format &= ~XMP_FORMAT_UNSIGNED;
	}

	if (format < 0) {
		format = PA_SAMPLE_S16NE;
	}

	ss.format = format;
	ss.channels = options->format & XMP_FORMAT_MONO ? 1 : 2;
	ss.rate = options->rate;

	s = pa_simple_new(NULL,		/* Use the default server */
		"xmp",			/* Our application's name */
		PA_STREAM_PLAYBACK,
		NULL,			/* Use the default device */
		"Music",		/* Description of our stream */
		&ss,			/* Our sample format */
		NULL,			/* Use default channel map */
		NULL,			/* Use default buffering attributes */
		&error);		/* Ignore error code */

	if (s == NULL) {
		fprintf(stderr, "pulseaudio error: %s\n", pa_strerror(error));
		return -1;
	}

	return 0;
}

static void play(void *b, int i)
{
	int j, error;

	if (downmix_24) {
		downmix_32_to_24_aligned(b, i);
	}

	do {
		if ((j = pa_simple_write(s, b, i, &error)) > 0) {
			i -= j;
			b = (char *)b + j;
		} else
			break;
	} while (i);

	if (j < 0) {
		fprintf(stderr, "pulseaudio error: %s\n", pa_strerror(error));
	}
}

static void deinit(void)
{
	pa_simple_free(s);
}

static void flush(void)
{
	int error;

	if (pa_simple_drain(s, &error) < 0) {
		fprintf(stderr, "pulseaudio error: %s\n", pa_strerror(error));
	}
}

static void onpause(void)
{
}

static void onresume(void)
{
}

static const char *description(void)
{
	return "PulseAudio sound output";
}

const struct sound_driver sound_pulseaudio = {
	"pulseaudio",
	NULL,
	description,
	init,
	deinit,
	play,
	flush,
	onpause,
	onresume
};
