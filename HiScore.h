// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include "settings.h"
#include "cdefs.h"
#include "FlashDrive.h"


// ********** Hiscore Table ****************

struct HiScore
{
	mutable char name[10]{"Megatokio"};	// not 0-terminated
	uint16 score   = 99;
	uint16 playtime = 99;
	int16 year = 2020;
	int8 month = 1, day = 1, hour = 0, minute = 0;	// when

	HiScore()=default;
	HiScore(cstr nam, uint16 score, uint16 secs, int16 y, int8 m, int8 d, int8 hr, int8 min);
	HiScore(cstr nam, uint16 score, uint16 secs, const datetime_t&);
};

struct HiScores : public Flash::FlashData
{
	static constexpr uint16 TYPE = 0x12d4;
	static constexpr uint nelem = (Flash::page_size-sizeof(Flash::FlashData))/sizeof(HiScore);
	HiScore hiscores[nelem];

	HiScores() : Flash::FlashData(TYPE,sizeof(HiScores)) {}

	bool isHiscore (uint score) { return score > hiscores[nelem-1].score; }

	void addHiscore (const HiScore&);
	void addHiscore (cstr name, uint16 score, uint16 seconds, int16 y, int8 m, int8 d, int8 hour, int8 minute);
	void addHiscore (cstr name, uint16 score, uint16 seconds, const datetime_t&);

	Flash::ErrNo readFromFlash();
	Flash::ErrNo writeToFlash();	// caller must stop core1!

private:
	bool is_valid();
	void check_table();
};


inline void HiScores::addHiscore (cstr name, uint16 score, uint16 seconds, int16 y, int8 m, int8 d, int8 hour, int8 minute)
{
	addHiscore(HiScore(name,score,seconds,y,m,d,hour,minute));
}

inline void HiScores::addHiscore (cstr name, uint16 score, uint16 seconds, const datetime_t& t)
{
	addHiscore(HiScore(name,score,seconds,t));
}




