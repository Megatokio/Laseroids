// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include "utilities.h"
#include <math.h>
#include "cdefs.h"


static int32 parseInteger(const char* bu, uint& i, bool& delta, bool& valid)
{
	int vz = 1;
	int32 n = 0;
	delta = false;
	valid = false;

	for (;bu[i];i++) // detect + /- and skip garbage
	{
		char c = bu[i];
		if (c>='0' && c<='9') break;
		if (c>='@') break;
		if (c=='+') { delta = true; }
		if (c=='-') { delta = true; vz=-1; }
	}
	for (;bu[i];i++)
	{
		char c = bu[i];
		if (c<'0' || c>'9') break;
		valid = true;
		n = n*10+c-'0';
	}

	return n * vz;
}

static int32 parseInteger(const char* bu, uint& i, int32 dflt)
{
	bool delta,valid;
	int32 n = parseInteger(bu,i,delta,valid);
	return valid ? delta ? dflt + n : n : dflt;
}

int32 parseInteger(const char* bu, uint& i, int32 min, int32 dflt, int32 max)
{
	return minmax(min, parseInteger(bu,i,dflt), max);
}

static FLOAT parseFloat(const char* bu, uint& i, bool& delta, bool& valid)
{
	int vz=1;
	FLOAT n=0;
	uint32 f=0;
	delta = false;
	valid = false;

	for (;bu[i];i++) // detect + /- and skip garbage
	{
		char c = bu[i];
		if (c>='0' && c<='9') break;
		if (c>='@') break;
		if (c=='+') { delta = true; }
		if (c=='-') { delta = true; vz=-1; }
	}

	for (;bu[i];i++)
	{
		char c = bu[i];
		if (c=='.') { f=1; continue; }
		if (c<'0' || c>'9') break;
		valid = true;
		n = n*10 + (c-'0');
		f = f*10;
	}

	return n / (f?f:1) * vz;
}

static FLOAT parseFloat(const char* bu, uint& i, FLOAT dflt)
{
	bool delta,valid;
	FLOAT n = parseFloat(bu,i,delta,valid);
	return valid ? delta ? dflt + n : n : dflt;
}

FLOAT parseFloat(const char* bu, uint& i, FLOAT min, FLOAT dflt, FLOAT max)
{
	return minmax(min, parseFloat(bu,i,dflt), max);
}


int compare (const datetime_t& A, const datetime_t& B)
{
	// Compare two datetime_t
	//  neg if A is before B
	//  pos if B is before A
	//  0 if same

	#define test(N) if (A.N - B.N) return A.N - B.N
	test(year);
	test(month);
	test(day);
	test(hour);
	test(min);
	test(sec);
	#undef test
	return 0;
}
