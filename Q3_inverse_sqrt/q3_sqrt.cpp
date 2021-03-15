#include "standard_types.h"
#include <pico/stdlib.h>
#include <stdlib.h>			// rand()


template<uint iter=0>
inline float q3_inverse_sqrt (float x)
{
	// the Quake III algorithm for 1/sqrt(x)
	// finds the first approximation for sqrt(x)
	// by simply shifting the exponent of the float right by 1
	// and the 1/sqrt(x) by taking it's negative.

	// it only works on processors which do the INTs and the FLOATs like Intel:
	// little endian, use complements for negative INTs and IEEE754 4-byte floats.

	// Speed test on Raspberry Pico, compared to built-in 1.0f/sqrt(x):
	// 0 Iter:   6% execution time, accuracy ~3%
	// 1 Iter: 256% execution time, accuracy ~0.15%
	// 2 Iter: 476% execution time, accuracy ~0.0005%
	// 3 Iter: 676% execution time, accuracy ~0.000015%
	// -->
	// no need to use this except for a very fast first estimation!

	using iptr = int*;
	using fptr = float*;

	float x2 = x * 0.5f;		// x/2 needed for Newton's
	float y = x;				// y for the result

	int32 i;
	i = *iptr(&y);				// approximating:
	i = 0x5f3759df - (i>>1);	// log(1/sqrt(x)) = -1/2*log(x)
	y = *fptr(&i);				// 0x5f3759df mostly corrects offsets

	for (uint i=0; i<iter; i++)
		y *= 1.5f - x2*y*y;		// Newton's Iteration

	return y;
}



inline FLOAT rand (FLOAT max)
{
	return FLOAT(rand()) * max / FLOAT(RAND_MAX);
}

static void test_q3_inverse_sqrt()
{
	srand(time_us_32());
	float n[100];
	float s[100];
	float t[100];
	float u[100];
	float v[100];
	float w[100];

	for (uint i=0; i<100; i++) { n[i] = rand(75000.0f*75000.0f); }

	uint32 t0 = time_us_32();
	for (uint i=0; i<100; i++) { s[i] = 1.0f / sqrt(n[i]); }
	uint32 t1 = time_us_32();
	for (uint i=0; i<100; i++) { t[i] = q3_inverse_sqrt<0>(n[i]); }
	uint32 t2 = time_us_32();
	for (uint i=0; i<100; i++) { u[i] = q3_inverse_sqrt<1>(n[i]); }
	uint32 t3 = time_us_32();
	for (uint i=0; i<100; i++) { v[i] = q3_inverse_sqrt<2>(n[i]); }
	uint32 t4 = time_us_32();
	for (uint i=0; i<100; i++) { w[i] = q3_inverse_sqrt<3>(n[i]); }
	uint32 t5 = time_us_32();

	printf("time sqrt=%uus, q3<0>=%uus, q3<1>=%uus, q3<2>=%uus, q3<3>=%uus\n", t1-t0, (t2-t1), (t3-t2), (t4-t3), (t5-t4));
	printf("time sqrt=%uus, q3<0>=%u%%, q3<1>=%u%%, q3<2>=%u%%, q3<3>=%u%%\n", t1-t0, (t2-t1)*100/(t1-t0), (t3-t2)*100/(t1-t0), (t4-t3)*100/(t1-t0), (t5-t4)*100/(t1-t0));

	printf("deviation:\n");
	for (uint i=0; i<100; i++)
	{
		float d0 = t[i] / s[i] * 100 - 100;
		float d1 = u[i] / s[i] * 100 - 100;
		float d2 = v[i] / s[i] * 100 - 100;
		float d3 = w[i] / s[i] * 100 - 100;

		printf("<0>: %+.6f%%, <1>: %+.6f%%, <2>: %+.6f%%, <3>: %+.6f%%\n",double(d0),double(d1),double(d2),double(d3));
	}
	printf("\n");
}




