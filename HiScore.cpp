// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include "HiScore.h"
#include "cdefs.h"
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include <new>


HiScore::HiScore(cstr nam, uint16 score, uint16 secs, int16 y, int8 m, int8 d, int8 hr, int8 min) :
	score(score),playtime(secs),year(y),month(m),day(d),hour(hr),minute(min)
{
	strncpy(name,nam,sizeof(name));
}

HiScore::HiScore (cstr nam, uint16 score, uint16 secs, const datetime_t& t) :
	score(score),playtime(secs),
	year(t.year),month(t.month),day(t.day),
	hour(t.hour),minute(t.min)
{
	strncpy(name,nam,sizeof(name));
}


inline bool HiScores::is_valid()
{
	return fd_magic==FD_MAGIC && type==TYPE && size==sizeof(HiScores);
}

inline void HiScores::check_table()
{
	for (uint i=1; i<nelem; i++)
	{
		if (hiscores[i].score > hiscores[i-1].score)
			new(&hiscores[i]) HiScore();
	}
}

void HiScores::addHiscore (const HiScore& hs)
{
	// is it a high score?
	if (hs.score <= hiscores[nelem-1].score) return;

	// trim name:
	const ptr  cp = hs.name;
	const uint len = NELEM(hs.name);
	for (uint i=0; i<len; i++) if (uchar(cp[i])<' ') { cp[i] = ' '; }
	for (uint i=len; i && cp[--i] == ' '; ) { cp[i] = 0; }
	while (cp[0] == ' ') { memmove(cp,cp+1,len-1); cp[len-1] = 0; }

	// if player already has a hiscore then update this:
	for (uint i=0; i<nelem; i++)
	{
		if (memcmp(hs.name,hiscores[i].name,NELEM(hs.name))) continue; // not this player
		if (hs.score <= hiscores[i].score) return;			 // player's old hiscore is better
		break; // => update with current meta data
	}

	// find position in table and shift lower scores down:
	uint i = nelem-1;
	while (i && hs.score > hiscores[i-1].score)
	{
		hiscores[i] = hiscores[i-1];
		i--;
	}

	// add it:
	if (*hs.name == 0) strcpy(hs.name,"anonymous");
	hiscores[i] = hs;
	printf("new hiscore added: %s %u in %usec at %u:%02u\n",
		   hs.name, hs.score, hs.playtime, hs.hour, hs.minute);
}


Flash::ErrNo HiScores::readFromFlash()
{
	Flash::ErrNo err = Flash::readFlashData(this,TYPE);
	if (!is_valid()) { if (!err) err=Flash::DATA_CORRUPTED; new(this) HiScores(); }
	check_table();
	return err;
}

Flash::ErrNo HiScores::writeToFlash()
{
	return Flash::writeFlashData(this);
}

void HiScores::print() const
{
	printf("*** HISCORES ***\n");
	for (uint i=0; i<NELEM(hiscores); i++)
	{
		printf("%2u:",i);
		hiscores[i].print();
	}
	printf("\n");
}
















