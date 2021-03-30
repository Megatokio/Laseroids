// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#pragma once
#include <stdio.h>
#include "pico/stdlib.h"
#include "cdefs.h"
#include "hardware/flash.h"


extern char __flash_binary_end;


namespace Flash
{
	static constexpr uint sector_mask = FLASH_SECTOR_SIZE - 1;
	static constexpr uint page_mask   = FLASH_PAGE_SIZE - 1;

	static constexpr uint32 total_size = PICO_FLASH_SIZE_BYTES;
	static const ptr start = ptr(XIP_BASE);
	static const ptr end   = start + total_size;
	static const ptr start_nocache = ptr(XIP_NOCACHE_NOALLOC_BASE);
	static const ptr end_nocache   = start_nocache + total_size;

	static constexpr uint page_size   = FLASH_PAGE_SIZE; 	// for writing: 256
	static constexpr uint total_pages = total_size/page_size;

	static constexpr uint sector_size   = FLASH_SECTOR_SIZE; 	// for erasing: 4096
	static constexpr uint total_sectors = total_size/sector_size;

	static constexpr uint pages_per_sector = sector_size / page_size;


	static const uint32 program_size = uintptr_t(&__flash_binary_end) - XIP_BASE; // helper
	static const uint32 padded_program_size = (program_size+sector_mask) & ~sector_mask; // helper


	static const uint first_page = padded_program_size / page_size;
	static const uint last_page  = total_pages - 1;


	enum ErrNo { OK=0, INVALID_PAGE, INVALID_COUNT, FLASH_WRITE_ERROR,
				 NOT_FOUND, DATA_CORRUPTED, WRONG_SIZE, OUT_OF_SPACE };


	extern bool isEmptyPage(uint n);
	extern uint lowestUsedPage();
	extern uint topFreePage();

	extern ErrNo readPages  (uint page, char* data, uint bytecount);		// always returns OK
	extern int comparePages (uint page, const char* data, uint count);		// memcmp(data,flash)

	// write and erase:
	// interrupts on the current core are disabled during operation
	// the other core must be stopped by caller if running

	extern void write_page  (uint page, const char* data);					// no error check
	extern void write_pages (uint page, const char* data, uint size);		// no error check

	extern ErrNo writePages (uint page, const char* data, uint size, bool verify=true);
	extern ErrNo erasePages (uint page, uint count=pages_per_sector, bool verify=true);	// both must be multiple of pages_per_sector


	// templates to read/write/verify structured data and arrays thereof:

	template<typename T> ErrNo readPages (uint page, T* data, uint count=1)
	{
		return readPages(page, reinterpret_cast<char*>(data), sizeof(T)*count);
	}

	template<typename T> ErrNo writePages (uint page, const T* data, uint count=1, bool verify=true)
	{
		return writePages (page, reinterpret_cast<const char*>(data), sizeof(T)*count, verify);
	}

	template<typename T> int comparePages (uint page, const T* data, uint count=1)
	{
		return comparePages(page, reinterpret_cast<char*>(data), sizeof(T)*count);
	}


	// Minimal File System:
	// FlashData is stored at the end of Flash.
	// new data is written into space below old data.
	// each type_idf can only be stored once: the lowest data is it!
	// TODO: garbage collection

	class FlashData
	{
	protected:
		FlashData (uint16 type_idf, uint32 size) : type(type_idf), size(size) {}

	public:
		static constexpr uint16 FD_MAGIC = 0x2BADu;
		static constexpr uint32 MAX_SIZE = 256*1024;	// 1024 pages

		bool isValid() const { return fd_magic == FD_MAGIC && (size-8u) <= MAX_SIZE-8u; }

	public:
		const uint16 fd_magic = FD_MAGIC;
		const uint16 type;
		uint32 size;
	};

	// find page_num for FlashData type. returns 0 if not found. optionally store size of data found.
	extern uint findFlashData (uint16 type_idf, uint32* size = nullptr);

	// read FlashData. optionally read less: caller must check dest->size
	extern ErrNo readFlashData (FlashData* dest, uint16 type_idf, uint32 max_size, bool var_size=false);

	// write FlashData.
	extern ErrNo writeFlashData (const FlashData* source, bool verify=true);

	template<typename T>
	ErrNo readFlashData (T* dest, uint16 type_idf)
	{
		return readFlashData(static_cast<FlashData*>(dest), type_idf, sizeof(T));
	}
}


























