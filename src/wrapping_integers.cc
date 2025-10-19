#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint32_t offset = raw_value_ - zero_point.raw_value_;
  uint64_t candidate = offset + ( checkpoint & 0xFFFFFFFF00000000 );

  const uint64_t TWO_POW_32 = 1ULL << 32;
  const uint64_t TWO_POW_31 = 1ULL << 31;

  if ( candidate > checkpoint and candidate - checkpoint > TWO_POW_31) return candidate - TWO_POW_32;
  if ( candidate < checkpoint and checkpoint - candidate > TWO_POW_31) return candidate + TWO_POW_32;

  return candidate;
}
