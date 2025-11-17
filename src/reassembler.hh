#pragma once

#include "byte_stream.hh"

#include <cstdint>
#include <map>
#include <vector>

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output )
    : output_( std::move( output ) ),
      ring_buffer_(),
      filled_segments_()
  {}
  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  // This function is for testing only; don't add extra state to support it.
  uint64_t count_bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:
  ByteStream output_;

  uint64_t first_unpopped_index_{0};      // 已弹出到 ByteStream 的最小绝对下标
  uint64_t first_unassembled_index_{0};   // 下一块必须写入 ByteStream 的绝对下标
  uint64_t first_unacceptable_index_{0};  // 超出窗口的第一位置（丢弃超过它的数据）
  uint64_t eof_index_{0};                 // EOF 出现的位置
  bool eof_received_{false};

  bool ring_initialized_{false};          // 是否已经按照容量初始化环形缓冲区
  uint64_t total_capacity_{0};            // 当前窗口（缓冲+可写）容量
  size_t ring_size_{0};                   // 环形缓冲区实际大小（capacity+1）
  size_t front_pos_{0};                   // 环形缓冲区对应 first_unassembled_index_ 的位置
  std::vector<char> ring_buffer_;         // 存放连续已知字节的环形缓冲区

  std::map<uint64_t, uint64_t> filled_segments_;  // 保存 [start,end) 的已知区间，按绝对下标排序
  uint64_t pending_bytes_{0};                     // 仍未写入 ByteStream 的字节总数

  void initialize_ring(uint64_t capacity);                       // 按当前容量创建环形缓存
  void copy_into_ring(uint64_t absolute_index, const char* data, size_t length); // 将数据映射到环形缓冲区
  void add_interval(uint64_t start, uint64_t end);               // 在 map 中加入并合并区间
  void flush_contiguous(Writer& writer);                         // 将连续可写数据推入 ByteStream
};
