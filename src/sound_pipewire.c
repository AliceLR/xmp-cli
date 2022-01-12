/* Extended Module Player
 * Copyright (C) 1996-2016 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See the COPYING
 * file for more information.
 */

#include <unistd.h>
#include <pipewire/pipewire.h>
#include <pipewire/stream.h>
#include <pipewire/thread-loop.h>
#include <spa/param/audio/format-utils.h>
#include "sound.h"

static struct pw_thread_loop *loop;
static struct pw_stream *s;
static struct pw_stream_events ev;
static int channels;

static void event_process(void *priv)
{
	pw_thread_loop_signal(loop, 0);
}

static int init(struct options *options)
{
	uint8_t buffer[1024];
	struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	const struct spa_pod *params[1];
	struct pw_properties *properties;
	int argc = 0;

	pw_init(&argc, NULL);

	loop = pw_thread_loop_new("xmp-pw", NULL);
	if (loop == NULL) {
		goto err;
	}
	pw_thread_loop_lock(loop);

	if (pw_thread_loop_start(loop) != 0) {
		goto err_loop;
	}

	ev.version = PW_VERSION_STREAM_EVENTS;
	ev.process = event_process;

	/* Required for autoconnection */
	properties = pw_properties_new(
		PW_KEY_MEDIA_TYPE, "Audio",
		PW_KEY_MEDIA_CATEGORY, "Playback",
		PW_KEY_MEDIA_ROLE, "Music",
		NULL);

	s = pw_stream_new_simple(
		pw_thread_loop_get_loop(loop),	/* Loop */
		"xmp",				/* Application name */
		properties,			/* Properties */
		&ev,				/* Stream callbacks */
		NULL);				/* Private pointer */

	if (s == NULL) {
		goto err_loop;
	}

	options->format &= ~(XMP_FORMAT_UNSIGNED | XMP_FORMAT_8BIT);
	channels = options->format & XMP_FORMAT_MONO ? 1 : 2;

	/* Specify stream format
	 * Note: not sure there's a good way around the designated specifiers. */
	params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat,
		&SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_S16_OE,
			.channels = channels,
			.rate = options->rate));

	pw_stream_connect(s,			/* Stream */
		PW_DIRECTION_OUTPUT,		/* xmp is generating data */
		PW_ID_ANY,			/* Node */
		PW_STREAM_FLAG_AUTOCONNECT,	/* Flags */
		params, 1);			/* Params */

	pw_thread_loop_unlock(loop);
	return 0;

    err_loop:
	pw_thread_loop_unlock(loop);
	pw_thread_loop_stop(loop);
	pw_thread_loop_destroy(loop);
    err:
	pw_deinit();
	return -1;
}

static void play(void *b, int i)
{
	short *out = (short *)b;
	struct pw_buffer *buf;
	struct spa_buffer *sbuf;
	int timeout = 5;
	int frames_out;
	int stride;

	pw_thread_loop_lock(loop);

	while (i > 0) {
		while ((buf = pw_stream_dequeue_buffer(s)) == NULL) {
			/* Wait for an available buffer. */
			pw_thread_loop_timed_wait(loop, 1);
			pw_thread_loop_lock(loop);
			if (timeout-- <= 0)
				return;
		}

		sbuf = buf->buffer;
		stride = sizeof(short) * channels;
		frames_out = sbuf->datas[0].maxsize / stride;
		if(frames_out > i)
			frames_out = i;

		memcpy(sbuf->datas[0].data, out, frames_out * stride);
		out += frames_out;
		i -= frames_out;

		sbuf->datas[0].chunk->offset = 0;
		sbuf->datas[0].chunk->size = frames_out;
		sbuf->datas[0].chunk->stride = stride;

		pw_stream_queue_buffer(s, buf);
	}

	/* TODO wait for an available buffer here too? */
	pw_thread_loop_unlock(loop);
}

static void deinit(void)
{
	pw_thread_loop_stop(loop);
	pw_thread_loop_destroy(loop);
	pw_stream_destroy(s);
	pw_deinit();
}

static void flush(void)
{
}

static void onpause(void)
{
}

static void onresume(void)
{
}

struct sound_driver sound_pipewire = {
	"pipewire",
	"PipeWire sound output",
	NULL,
	init,
	deinit,
	play,
	flush,
	onpause,
	onresume
};
