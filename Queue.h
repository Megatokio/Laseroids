/*	Copyright  (c)	Günter Woigk 2020 - 2021
					mailto:kio@little-bat.de

	This file is free software.

	Permission to use, copy, modify, distribute, and sell this software
	and its documentation for any purpose is hereby granted without fee,
	provided that the above copyright notice appears in all copies and
	that both that copyright notice, this permission notice and the
	following disclaimer appear in supporting documentation.

	THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT ANY WARRANTY,
	NOT EVEN THE IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR
	A PARTICULAR PURPOSE, AND IN NO EVENT SHALL THE COPYRIGHT HOLDER
	BE LIABLE FOR ANY DAMAGES ARISING FROM THE USE OF THIS SOFTWARE,
	TO THE EXTENT PERMITTED BY APPLICABLE LAW.
*/

#pragma once
#include "cdefs.h"
#include <hardware/sync.h>



//	Queue for sending data from one sender to one concurrently running receiver.
//
//	The queue size (element count) must be a power of 2.
//	Reading and writing involves no mutex.
//	There is no synchronization on the sender side and there is no synchronization on the receiver side.
//	If more than 1 thread can read and/or write to the queue, then this template can't be used.
//
//	2021-03: reverted to volatile because __atomic_fetch_add_4 is missing. :-/
//			 using __dmb() (data memory barrier) to ensure that index is written back after data

template<typename T, uint SIZE>
struct Queue
{
	static const uint MASK = SIZE - 1;
	static_assert(SIZE > 0 && (SIZE & MASK) == 0, "size must be a power of 2");

protected:
	T buffer[SIZE];				// write -> wp++ -> read -> rp++
	volatile uint rp = 0;		// only modified by reader
	volatile uint wp = 0;		// only modified by writer

	static inline void copy (T* z, const T* q, uint n)	// helper: hopefully optimized proper copy
	{
		for (uint i=0; i<n; i++) { z[i] = q[i]; }
	}

	void copy_q2b(T*, uint n) noexcept;			// helper: copy queue to external linear buffer
	void copy_b2q(const T*, uint n) noexcept;	// helper: copy external linear buffer to queue

public:
	Queue() = default;		///< The default and only c'tor.
	~Queue() = default;		///< D'tor. Does nothing.

	/**
	 * Get the number of available elements in the queue which can be read without blocking.
	 */
	uint avail() const noexcept { return wp - rp; }

	/**
	 * Get the number of free elements in the queue which can be written without blocking.
	 */
	uint free() const noexcept { return SIZE - avail(); }

	/**
	 * Read one element.
	 * There must be at least 1 element @ref avail().
	 */
	T getc()        { assert(avail()); uint i=rp; T c = buffer[i++&MASK]; __dmb(); rp=i; return c; }

	/**
	 * Write one element.
	 * there must be at least 1 element @ref free()
	 */
	void putc (T c) { assert(free()); uint i=wp; buffer[i++&MASK] = c; __dmb(); wp=i; }

	/**
	 * Read multiple elements.
	 * Read at most @ref n elements into the buffer at @ z. @ref read() reads and returns
	 * as many elements as available.
	 *
	 * @param z  pointer to the receiving buffer.
	 * @param n  size (element count) of the buffer.
	 * @return   number of elements actually read.
	 */
	uint read (T* z, uint n) noexcept { n = min(n,avail()); copy_q2b(z,n); __dmb(); rp+=n; return n; }

	/**
	 * Write multiple elements.
	 * Write at most @ref n elements from buffer at @ q. @ref write() writes and returns
	 * as many elements as possible.
	 *
	 * @param q  pointer to the data to write.
	 * @param n  size (element count) of data to write.
	 * @return   number of elements actually written.
	 */
	uint write (const T* q, uint n) noexcept { n = min(n,free()); copy_b2q(q,n); __dmb(); wp+=n; return n; }
};


template<typename T, uint SIZE>
inline void Queue<T,SIZE>::copy_b2q (const T* q, uint n) noexcept
{
	uint wp = this->wp & MASK;
	T* z = buffer + wp;
	uint n1 = SIZE - wp;

	if (n <= n1)
	{
		copy(z, q, n);
	}
	else
	{
		copy(z, q, n1);
		copy(buffer,q+n1,n-n1);
	}
}

template<typename T, uint SIZE>
inline void Queue<T,SIZE>::copy_q2b (T* z, uint n) noexcept
{
	uint rp = this->rp & MASK;
	const T* q = buffer + rp;
	uint n1 = SIZE - rp;

	if (n <= n1)
	{
		copy(z, q, n);
	}
	else
	{
		copy(z, q, n1);
		copy(z+n1,buffer,n-n1);
	}
}


