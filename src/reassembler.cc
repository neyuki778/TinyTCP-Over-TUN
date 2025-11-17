#include "reassembler.hh"
#include "debug.hh"

#include <algorithm>
#include <iterator>
#include <string>

using namespace std;

// 入口：接收一个分段并尝试立刻写入 ByteStream
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer = output_.writer();
  Reader& reader = output_.reader();

  const uint64_t buffered_in_byte_stream = reader.bytes_buffered();
  const uint64_t available_capacity = writer.available_capacity();
  const uint64_t segment_end_index = first_index + data.length();

  first_unpopped_index_ = reader.bytes_popped();
  first_unassembled_index_ = first_unpopped_index_ + buffered_in_byte_stream;
  first_unacceptable_index_ = first_unassembled_index_ + available_capacity;

  const uint64_t total_capacity = buffered_in_byte_stream + available_capacity;
  if (not ring_initialized_ && total_capacity > 0) {
    initialize_ring( total_capacity );
  }

  if ( is_last_substring ) {
    eof_received_ = true;
    eof_index_ = segment_end_index;
  }

  if ( not ring_initialized_ || segment_end_index <= first_unassembled_index_ || data.empty() ) {
    flush_contiguous( writer );
    return;
  }

  const uint64_t clipped_start = max( first_index, first_unassembled_index_ );
  const uint64_t clipped_end = min( segment_end_index, first_unacceptable_index_ );

  if ( clipped_start < clipped_end ) {
    const size_t offset_in_data = clipped_start - first_index;
    const size_t len = clipped_end - clipped_start;
    copy_into_ring( clipped_start, data.data() + offset_in_data, len );
    add_interval( clipped_start, clipped_end );
  }

  flush_contiguous( writer );
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  return pending_bytes_;
}

// 按照当前窗口容量初始化环形缓冲区（capacity+1 防止满与空状态冲突）
void Reassembler::initialize_ring( uint64_t capacity )
{
  total_capacity_ = capacity;
  ring_size_ = static_cast<size_t>( total_capacity_ + 1 );
  ring_buffer_.assign( ring_size_, '\0' );
  front_pos_ = 0;
  ring_initialized_ = true;
}

// 将 [absolute_index, absolute_index+length) 的字节写入环形缓冲区中对应位置
void Reassembler::copy_into_ring( uint64_t absolute_index, const char* data, size_t length )
{
  if ( not ring_initialized_ || length == 0 ) {
    return;
  }

  size_t offset = static_cast<size_t>( absolute_index - first_unassembled_index_ );
  size_t pos = ( front_pos_ + offset ) % ring_size_;
  size_t remaining = length;
  size_t copied = 0;

  while ( remaining > 0 ) {
    const size_t chunk = min( remaining, ring_size_ - pos );
    std::copy_n( data + copied, chunk, ring_buffer_.data() + pos );
    pos = ( pos + chunk ) % ring_size_;
    copied += chunk;
    remaining -= chunk;
  }
}

// 记录一个新的已知区间，并与相邻区间合并，维护 pending 计数
void Reassembler::add_interval( uint64_t start, uint64_t end )
{
  if ( start >= end ) {
    return;
  }

  auto it = filled_segments_.lower_bound( start );
  if ( it != filled_segments_.begin() ) {
    auto prev = std::prev( it );
    if ( prev->second >= start ) {
      it = prev;
    }
  }

  uint64_t new_start = start;
  uint64_t new_end = end;

  while ( it != filled_segments_.end() && it->first <= new_end ) {
    if ( it->second < new_start ) {
      ++it;
      continue;
    }

    new_start = min( new_start, it->first );
    new_end = max( new_end, it->second );
    pending_bytes_ -= ( it->second - it->first );
    it = filled_segments_.erase( it );
  }

  filled_segments_.emplace( new_start, new_end );
  pending_bytes_ += ( new_end - new_start );
}

// 将 map 中以 first_unassembled_index_ 开头的连续区间写入 ByteStream
void Reassembler::flush_contiguous( Writer& writer )
{
  if ( not ring_initialized_ ) {
    if ( eof_received_ && first_unassembled_index_ == eof_index_ ) {
      writer.close();
    }
    return;
  }

  while ( not filled_segments_.empty() ) {
    auto it = filled_segments_.begin();
    if ( it->first != first_unassembled_index_ ) {
      break;
    }

    const uint64_t start = it->first;
    const uint64_t end = it->second;
    const size_t len = end - start;

    string chunk;
    chunk.reserve( len );

    size_t offset = 0;
    while ( offset < len ) {
      const size_t pos = ( front_pos_ + offset ) % ring_size_;
      const size_t chunk_len = min( len - offset, ring_size_ - pos );
      chunk.append( ring_buffer_.data() + pos, chunk_len );
      offset += chunk_len;
    }

    writer.push( chunk );
    pending_bytes_ -= len;
    first_unassembled_index_ = end;
    front_pos_ = ( front_pos_ + len ) % ring_size_;
    filled_segments_.erase( it );
  }

  if ( eof_received_ && first_unassembled_index_ == eof_index_ ) {
    writer.close();
  }
}
