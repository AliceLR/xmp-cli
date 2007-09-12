/* SoundSmith/MegaTracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: ssmt_load.c,v 1.7 2007-09-12 03:20:59 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Format specs from MTPng 4.3.1 by Ian Schmitd and deMODifier by Bret Victor
 * http://home.cfl.rr.com/ischmidt/warez.html
 * http://worrydream.com/media/demodifier.tgz
 */

/* From the deMODifier readme:
 *
 * SoundSmith was arguably the most popular music authoring tool for the
 * Apple IIgs.  Introduced in the IIgs's heyday (which was, accurately 
 * enough, just about one day), this software inspired the creation
 * of countless numbers of IIgs-specific tunes, several of which were 
 * actually worth listening to.  
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "load.h"
#include "asif.h"

#define NAME_SIZE 255


static void split_name(char *s, char **d, char **b)
{
	if ((*b = strrchr(s, '/'))) {
		**b = 0;
		(*b)++;
		*d = s;
	} else {
		*d = "";
		*b = s;
	}
}


int mtp_load(FILE * f)
{
	struct xxm_event *event;
	int i, j, k;
	uint8 buffer[25];
	int blocksize;
	char *dirname, *basename;
	char filename[NAME_SIZE];
	char modulename[NAME_SIZE];
	FILE *s;

	LOAD_INIT();

	fread(buffer, 6, 1, f);

	if (!memcmp(buffer, "SONGOK", 6))
		strcpy(xmp_ctl->type, "IIgs SoundSmith");
	else if (!memcmp(buffer, "IAN92a", 8))
		strcpy(xmp_ctl->type, "IIgs MegaTracker");
	else
		return -1;

	strncpy(modulename, xmp_ctl->filename, NAME_SIZE);
	split_name(modulename, &dirname, &basename);

	blocksize = read16l(f);
	xxh->tpo = read16l(f);
	fseek(f, 10, SEEK_CUR);		/* skip 10 reserved bytes */
	
	xxh->ins = xxh->smp = 15;
	INSTRUMENT_INIT();

	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		fread(buffer, 1, 22, f);
		if (buffer[0]) {
			buffer[buffer[0] + 1] = 0;
			copy_adjust(xxih[i].name, buffer + 1, 22);
		}
		read16l(f);		/* skip 2 reserved bytes */
		xxi[i][0].vol = read8(f) >> 2;
		xxi[i][0].pan = 0x80;
		fseek(f, 5, SEEK_CUR);	/* skip 5 bytes */
	}

	xxh->len = read8(f) & 0x7f;
	read8(f);
	fread(xxo, 1, 128, f);

	MODULE_INFO();

	fseek(f, 600, SEEK_SET);

	xxh->chn = 14;
	xxh->pat = blocksize / (14 * 64);
	xxh->trk = xxh->pat * xxh->chn;

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(0, "Stored patterns: %d ", xxh->pat);

	/* Load notes */
	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < xxh->chn; k++) {
				event = &EVENT(i, k, j);
				event->note = read8(f);;
				if (event->note)
					event->note += 12;
			}
		}
		reportv(0, ".");
	}

	/* Load fx1 */
	for (i = 0; i < xxh->pat; i++) {
		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < xxh->chn; k++) {
				uint8 x;
				event = &EVENT(i, k, j);
				x = read8(f);;
				event->ins = x >> 4;

				switch (x & 0x0f) {
				case 0x00:
					event->fxt = FX_ARPEGGIO;
					break;
				case 0x03:
					event->fxt = FX_VOLSET;
					break;
				case 0x05:
					event->fxt = FX_VOLSLIDE_DN;
					break;
				case 0x06:
					event->fxt = FX_VOLSLIDE_UP;
					break;
				case 0x0f:
					event->fxt = FX_TEMPO;
					break;
				}
			}
		}
	}

	/* Load fx2 */
	for (i = 0; i < xxh->pat; i++) {
		for (j = 0; j < xxp[i]->rows; j++) {
			for (k = 0; k < xxh->chn; k++) {
				event = &EVENT(i, k, j);
				event->fxp = read8(f);;

				switch (event->fxt) {
				case FX_VOLSET:
				case FX_VOLSLIDE_DN:
				case FX_VOLSLIDE_UP:
					event->fxp >>= 2;
				}
			}
		}
	}

	reportv(0, "\n");

	/* Read instrument data */
	reportv(0, "Instruments    : %d ", xxh->ins);
	reportv(1, "\n     Name                   Len  LBeg LEnd L Vol");

	for (i = 0; i < xxh->ins; i++) {
		if (!xxih[i].name[0])
			continue;

		strncpy(filename, dirname, NAME_SIZE);
		if (*filename)
			strncat(filename, "/", NAME_SIZE);
		strncat(filename, (char *)xxih[i].name, NAME_SIZE);

		s = fopen(filename, "rb");
		asif_load(s, i);
		fclose(s);

#if 0
		xxs[i].lps = 0;
		xxs[i].lpe = 0;
		xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
		xxi[i][0].fin = 0;
		xxi[i][0].pan = 0x80;
#endif

		if (V(1) && (strlen((char*)xxih[i].name) || (xxs[i].len > 1))) {
			report("\n[%2X] %-22.22s %04x %04x %04x %c V%02x", i,
				xxih[i].name,
				xxs[i].len, xxs[i].lps, xxs[i].lpe,
				xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				xxi[i][0].vol);
		}
	}
	reportv(0, "\n");

	return 0;
}