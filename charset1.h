// Copyright (c) 2005 - 2021 kio@little-bat.de
// SPDX-License-Identifier: BSD-2-Clause


// ------------------------------------------------------------
// Character set from IC Tester
// ------------------------------------------------------------


#include "cdefs.h"

extern const uchar charset1[];
extern uint16 charset1_offsets[256];
extern void   charset1_init();
inline const uchar* charset1_get_glyph (char c) { return charset1 + charset1_offsets[uchar(c)]; }




