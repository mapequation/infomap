// MersenneTwister.h
// Mersenne Twister random number generator -- a C++ class MTRand
// Based on code by Makoto Matsumoto, Takuji Nishimura, and Shawn Cokus
// Richard J. Wagner  v0.8  24 March 2002  rjwagner@writeme.com

// The Mersenne Twister is an algorithm for generating random numbers.  It
// was designed with consideration of the flaws in various other generators.
// The period, 2^19937-1, and the order of equidistribution, 623 dimensions,
// are far greater.  The generator is also fast; it avoids multiplication and
// division, and it benefits from caches and pipelines.  For more information
// see the inventors' web page at http://www.math.keio.ac.jp/~matumoto/emt.html

// Reference
// M. Matsumoto and T. Nishimura, "Mersenne Twister: A 623-Dimensionally
// Equidistributed Uniform Pseudo-Random Number Generator", ACM Transactions on
// Modeling and Computer Simulation, Vol. 8, No. 1, January 1998, pp 3-30.

// Copyright (C) 2002  Richard J. Wagner
// 
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

// The original code included the following notice:
//
//     Copyright (C) 1997, 1999 Makoto Matsumoto and Takuji Nishimura.
//     When you use this, send an email to: matumoto@math.keio.ac.jp
//     with an appropriate reference to your work.
//
// It would be nice to CC: rjwagner@writeme.com and Cokus@math.washington.edu
// when you write.

#ifndef MERSENNETWISTER_H
#define MERSENNETWISTER_H

// Not thread safe (unless auto-initialization is avoided and each thread has
// its own MTRand object)

#include <iostream>
#include <limits.h>
#include <stdio.h>
#include <time.h>

class MTRand {
// Data
public:
	typedef unsigned long uint32;  // unsigned integer type, at least 32 bits
	
	enum { N = 624 };              // length of state vector
	enum { SAVE = N + 1 };         // length of array for save()

protected:
	enum { M = 397 };              // period parameter
	enum { MAGIC = 0x9908b0dfU };  // magic constant
	
	uint32 state[N];  // internal state
	uint32 *pNext;    // next value to get from state
	int left;         // number of values left before reload needed


//Methods
public:
	MTRand( const uint32& oneSeed );  // initialize with a simple uint32
	MTRand( uint32 *const bigSeed );  // initialize with an array of N uint32's
	MTRand();  // auto-initialize with /dev/urandom or time() and clock()
	
	// Access to 32-bit random numbers
	// Do NOT use for CRYPTOGRAPHY without securely hashing several returned
	// values together, otherwise the generator state can be learned after
	// reading 624 consecutive values.
	double rand();                          // real number in [0,1]
	double rand( const double& n );         // real number in [0,n]
	double randExc();                       // real number in [0,1)
	double randExc( const double& n );      // real number in [0,n)
	double randDblExc();                    // real number in (0,1)
	double randDblExc( const double& n );   // real number in (0,n)
	uint32 randInt();                       // integer in [0,2^32-1]
	uint32 randInt( const uint32& n );      // integer in [0,n] for n < 2^32
	double operator()() { return rand(); }  // same as rand()
	
	// Re-seeding functions with same behavior as initializers
	void seed( uint32 oneSeed );
	void seed( uint32 *const bigSeed );
	void seed();
	
	// Saving and loading generator state
	void save( uint32* saveArray ) const;  // to array of size SAVE
	void load( uint32 *const loadArray );  // from such array
	friend std::ostream& operator<<( std::ostream& os, const MTRand& mtrand );
	friend std::istream& operator>>( std::istream& is, MTRand& mtrand );

protected:
	void reload();
	uint32 hiBit( const uint32& u ) const { return u & 0x80000000U; }
	uint32 loBit( const uint32& u ) const { return u & 0x00000001U; }
	uint32 loBits( const uint32& u ) const { return u & 0x7fffffffU; }
	uint32 mixBits( const uint32& u, const uint32& v ) const
		{ return hiBit(u) | loBits(v); }
	uint32 twist( const uint32& m, const uint32& s0, const uint32& s1 ) const
		{ return m ^ (mixBits(s0,s1)>>1) ^ (loBit(s1) ? MAGIC : 0U); }
	static uint32 hash( time_t t, clock_t c );
};


inline MTRand::MTRand( const uint32& oneSeed )
	{ seed(oneSeed); }

inline MTRand::MTRand( uint32 *const bigSeed )
	{ seed(bigSeed); }

inline MTRand::MTRand()
	{ seed(); }

inline double MTRand::rand()
    { return double(randInt()) * 2.3283064370807974e-10; }

inline double MTRand::rand( const double& n )
	{ return rand() * n; }

inline double MTRand::randExc()
	{ return double(randInt()) * 2.3283064365386963e-10; }

inline double MTRand::randExc( const double& n )
	{ return randExc() * n; }

inline double MTRand::randDblExc()
	{ return double( 1.0 + randInt() ) * 2.3283064359965952e-10; }

inline double MTRand::randDblExc( const double& n )
	{ return randDblExc() * n; }

inline MTRand::uint32 MTRand::randInt()
{
	if( left == 0 ) reload();
	--left;
		
	register uint32 s1;
	s1 = *pNext++;
	s1 ^= (s1 >> 11);
	s1 ^= (s1 <<  7) & 0x9d2c5680U;
	s1 ^= (s1 << 15) & 0xefc60000U;
	return ( s1 ^ (s1 >> 18) );
}


inline MTRand::uint32 MTRand::randInt( const uint32& n )
{
	// Find which bits are used in n
	uint32 used = ~0;
	for( uint32 m = n; m; used <<= 1, m >>= 1 ) {}
	used = ~used;
	
	// Draw numbers until one is found in [0,n]
	uint32 i;
	do
		i = randInt() & used;  // toss unused bits to shorten search
	while( i > n );
	return i;
}


inline void MTRand::seed( uint32 oneSeed )
{
	// Seed the generator with a simple uint32
	register uint32 *s;
	register int i;
	for( i = N, s = state;
	     i--;
		 *s    = oneSeed & 0xffff0000,
		 *s++ |= ( (oneSeed *= 69069U)++ & 0xffff0000 ) >> 16,
		 (oneSeed *= 69069U)++ ) {}  // hard to read, but fast
	reload();
}


inline void MTRand::seed( uint32 *const bigSeed )
{
	// Seed the generator with an array of 624 uint32's
	// There are 2^19937-1 possible initial states.  This function allows
	// any one of those to be chosen by providing 19937 bits.  The lower
	// 31 bits of the first element, bigSeed[0], are discarded.  Any bits
	// above the lower 32 in each element are also discarded.  Theoretically,
	// the rest of the array can contain any values except all zeroes.
	// Just call seed() if you want to get array from /dev/urandom
	register uint32 *s = state, *b = bigSeed;
	register int i = N;
	for( ; i--; *s++ = *b++ & 0xffffffff ) {}
	reload();
}


inline void MTRand::seed()
{
	// Seed the generator with an array from /dev/urandom if available
	// Otherwise use a hash of time() and clock() values
	
	// First try getting an array from /dev/urandom
	FILE* urandom = fopen( "/dev/urandom", "rb" );
	if( urandom )
	{
		register uint32 *s = state;
		register int i = N;
		register bool success = true;
		while( success && i-- )
		{
			success = fread( s, sizeof(uint32), 1, urandom );
			*s++ &= 0xffffffff;  // filter in case uint32 > 32 bits
		}
		fclose(urandom);
		if( success )
		{
			// There is a 1 in 2^19937 chance that a working urandom gave
			// 19937 consecutive zeroes and will make the generator fail
			// Ignore that case and continue with initialization
			reload();
			return;
		}
	}
	
	// Was not successful, so use time() and clock() instead
	seed( hash( time(NULL), clock() ) );
}


inline void MTRand::reload()
{
	// Generate N new values in state
	// Made clearer and faster by Matthew Bellew (matthew.bellew@home.com)
	register uint32 *p = state;
	register int i;
	for( i = N - M; i--; ++p )
		*p = twist( p[M], p[0], p[1] );
	for( i = M; --i; ++p )
		*p = twist( p[M-N], p[0], p[1] );
	*p = twist( p[M-N], p[0], state[0] );

	left = N, pNext = state;
}


inline MTRand::uint32 MTRand::hash( time_t t, clock_t c )
{
	// Get a uint32 from t and c
	// Better than uint32(x) in case x is floating point in [0,1]
	// Based on code by Lawrence Kirby (fred@genesis.demon.co.uk)

	static uint32 differ = 0;  // guarantee time-based seeds will change

	uint32 h1 = 0;
	unsigned char *p = (unsigned char *) &t;
	for( size_t i = 0; i < sizeof(t); ++i )
	{
		h1 *= UCHAR_MAX + 2U;
		h1 += p[i];
	}
	uint32 h2 = 0;
	p = (unsigned char *) &c;
	for( size_t j = 0; j < sizeof(c); ++j )
	{
		h2 *= UCHAR_MAX + 2U;
		h2 += p[j];
	}
	return ( h1 + differ++ ) ^ h2;
}


inline void MTRand::save( uint32* saveArray ) const
{
	register uint32 *sa = saveArray;
	register const uint32 *s = state;
	register int i = N;
	for( ; i--; *sa++ = *s++ ) {}
	*sa = left;
}


inline void MTRand::load( uint32 *const loadArray )
{
	register uint32 *s = state;
	register uint32 *la = loadArray;
	register int i = N;
	for( ; i--; *s++ = *la++ ) {}
	left = *la;
	pNext = &state[N-left];
}


inline std::ostream& operator<<( std::ostream& os, const MTRand& mtrand )
{
	register const MTRand::uint32 *s = mtrand.state;
	register int i = mtrand.N;
	for( ; i--; os << *s++ << "\t" ) {}
	return os << mtrand.left;
}


inline std::istream& operator>>( std::istream& is, MTRand& mtrand )
{
	register MTRand::uint32 *s = mtrand.state;
	register int i = mtrand.N;
	for( ; i--; is >> *s++ ) {}
	is >> mtrand.left;
	mtrand.pNext = &mtrand.state[mtrand.N-mtrand.left];
	return is;
}

#endif  //MERSENNETWISTER_H

// Change log:
//
// v0.1 - First release on 15 May 2000
//      - Based on code by Makoto Matsumoto, Takuji Nishimura, and Shawn Cokus
//      - Translated from C to C++
//      - Made completely ANSI compliant
//      - Designed convenient interface for initialization, seeding, and
//        obtaining numbers in default or user-defined ranges
//      - Added automatic seeding from /dev/urandom or time() and clock()
//      - Provided functions for saving and loading generator state
//
// v0.2 - Fixed bug which reloaded generator one step too late
//
// v0.3 - Switched to clearer, faster reload() code from Matthew Bellew
//
// v0.4 - Removed trailing newline in saved generator format to be consistent
//        with output format of built-in types
//
// v0.5 - Improved portability by replacing static const int's with enum's and
//        clarifying return values in seed(); suggested by Eric Heimburg
//      - Removed MAXINT constant; use 0xffffffff instead
//
// v0.6 - Eliminated seed overflow when uint32 is larger than 32 bits
//      - Changed integer [0,n] generator to give better uniformity
//
// v0.7 - Fixed operator precedence ambiguity in reload()
//      - Added access for real numbers in (0,1) and (0,n)
//
// v0.8 - Included time.h header to properly support time_t and clock_t
