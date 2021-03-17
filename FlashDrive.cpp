// Copyright (c) 2021 Mathema GmbH
// SPDX-License-Identifier: BSD-3-Clause
// Author: Günter Woigk (Kio!)
// Copyright (c) 2021 kio@little-bat.de
// BSD 2-clause license

#include "FlashDrive.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"


namespace Flash
{

static uint last_free_page = last_page;	// cache

bool isEmptyPage (uint n)
{
	const uint32* a = reinterpret_cast<uint32*>(start_nocache + n*page_size);
	const uint32* e = a + page_size/sizeof(uint32);

	while (a<e) { if (*--e != 0xffffffffu) return false; }
	return true;
}

uint lowestUsedPage()
{
	while (last_free_page >= first_page)
	{
		if (isEmptyPage(last_free_page)) return last_free_page+1;
		last_free_page--;
	}
	return first_page;
}

uint topFreePage()
{
	while (last_free_page >= first_page)
	{
		if (isEmptyPage(last_free_page)) return last_free_page;
		last_free_page--;
	}
	return 0;	// no space
}


void write_page (uint page, const char* data)
{
	// store data into the given page which must be empty
	// interrupts on this core will be disabled during operation
	// the other core must be stopped by caller if running

	uint32 offset = page * page_size;
	uint32 oldmask = save_and_disable_interrupts();
	flash_range_program(offset, u8ptr(data), page_size);
	restore_interrupts(oldmask);
}

void write_pages (uint page, const char* data, uint count)
{
	// store data into the given pages which must be empty
	// interrupts on this core will be disabled during operation
	// the other core must be stopped by caller if running

	uint remainder = count & page_mask;
	count -= remainder;
	uint32 offset = page * page_size;

	if (count)
	{
		uint32 oldmask = save_and_disable_interrupts();
		flash_range_program(offset, u8ptr(data), count);
		restore_interrupts(oldmask);
	}

	if (remainder)
	{
		uchar buffer[page_size];
		memset(buffer,0xff,page_size);
		memcpy(buffer,data+count,remainder);

		uint32 oldmask = save_and_disable_interrupts();
		flash_range_program(offset+count, buffer, page_size);
		restore_interrupts(oldmask);
	}
}


ErrNo readPages (uint page, char* data, uint count)
{
	//if (page < first_page || page > last_page) return INVALID_PAGE;	who cares
	//if (page * page_size + count > total_size) return INVALID_COUNT;

	cptr a = start_nocache + page*page_size;
	memcpy(data, a, count);
	return OK;
}

ErrNo writePages (uint page, const char* data, uint count, bool verify)
{
	// store data into the given pages which must be empty
	// interrupts on this core will be disabled during operation
	// the other core must be stopped by caller if running

	if (page < first_page || page > last_page) return INVALID_PAGE;
	if (page * page_size + count > total_size) return INVALID_COUNT;

	write_pages (page, data, count);
	return verify && comparePages(page,data,count) ? FLASH_WRITE_ERROR : OK;
}

int comparePages (uint page, const char* data, uint count)
{
	//if (page < first_page || page > last_page) return INVALID_PAGE;	who cares
	//if (page * page_size + count > total_size) return INVALID_COUNT;

	uint32 offset = page * page_size;
	return memcmp(data, start_nocache+offset, count);
}

ErrNo erasePages (uint page, uint num_pages, bool verify)
{
	// flash-erase the given pages
	// interrupts on this core will be disabled during operation
	// the other core must be stopped by caller if running

	if (page < first_page || page > last_page || page % pages_per_sector) return INVALID_PAGE;
	if ((page+num_pages) > total_pages || num_pages % pages_per_sector) return INVALID_COUNT;

	uint32 oldmask = save_and_disable_interrupts();
	flash_range_erase(page*page_size,num_pages*page_size);
	restore_interrupts(oldmask);

	if (verify) while (num_pages--) { if (!isEmptyPage(page++)) return FLASH_WRITE_ERROR; }
	return OK;
}


uint findFlashData (uint16 type_idf, uint32* size)
{
	uint32 data_size;
	for (uint page = lowestUsedPage(); page <= last_page; page += (data_size+page_mask)/page_size)
	{
		cptr a = start_nocache + page*page_size;
		const FlashData* fd = reinterpret_cast<const FlashData*>(a);

		// data invalid => end of valid data in Flash => return not found:
		if (fd->fd_magic != FlashData::FD_MAGIC) return 0;		// not found
		data_size = fd->size; // read only once from Flash: uncached!
		if (data_size-8u > FlashData::MAX_SIZE-8u) return 0;	// not found

		if (fd->type == type_idf)
		{
			if (size) *size = data_size;
			return page;
		}
	}

	return 0; // not found
}

ErrNo readFlashData (FlashData* dest, uint16 type_idf, uint32 size, bool var_size)
{
	uint32 actual_size;
	uint page = findFlashData(type_idf, &actual_size);

	if (page == 0) return NOT_FOUND;
	if (var_size) size = min(size,actual_size);
	else if (size != actual_size) return WRONG_SIZE;
	return readPages(page,reinterpret_cast<char*>(dest),size);
}

ErrNo writeFlashData (const FlashData* source, bool verify)
{
	uint page = lowestUsedPage();
	uint num_pages = (source->size+page_mask)/page_size;

	if (page-num_pages < first_page) return OUT_OF_SPACE;
	page -= num_pages;

	return writePages(page,reinterpret_cast<const char*>(source),source->size,verify);

}


} // namespace












