## reassembler的v0实现:
```cpp
#include "reassembler.hh"
#include "debug.hh"
#include<vector>
#include<algorithm>

using namespace std;

void place_string_efficiently(std::vector<char>& container,
                              std::vector<bool>& present,
                              std::string data,
                              uint64_t start_index);

void try_close(bool recv, uint64_t idx, Writer& writer);

void try_push_assembled_bytes(std::vector<char>& unassembled_bytes,
                               std::vector<bool>& unassembled_present,
                               std::string& data_to_push,
                               Writer& writer,
                               uint64_t& first_unassembled_index); 

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  Writer& writer = output_.writer();
  Reader& reader = output_.reader();

  uint64_t buffered_in_byte_stream = reader.bytes_buffered(); 
  uint64_t popped_bytes_index = reader.bytes_popped();
  uint64_t available_capacity = writer.available_capacity();
  uint64_t last_index = first_index + data.length();

  first_unpopped_index_ = popped_bytes_index;
  first_unassembled_index_ = first_unpopped_index_ + buffered_in_byte_stream;
  first_unacceptable_index_ = first_unassembled_index_ + available_capacity;

  if (is_last_substring) {
    eof_index_ = last_index;
    eof_received_ = true;
  }

  // case0: stream index is too big or too late
  if ( first_index >= first_unacceptable_index_ or last_index < first_unassembled_index_) {
    try_close(eof_received_, eof_index_, writer);  
    return;
  }

  // cut data length
  if (first_index + data.length() > first_unacceptable_index_) {
    data = data.substr(0, first_unacceptable_index_ - first_index);
  }

  // init reassembler if need or reshape
  if ( unassembled_bytes.empty() && available_capacity > 0 ) {
    unassembled_bytes.resize(available_capacity);
    unassembled_present.assign(available_capacity, false);
  } else if (unassembled_bytes.size() != available_capacity && available_capacity > 0) {
    unassembled_bytes.resize(available_capacity);
    unassembled_present.resize(available_capacity, false);
  }

  // case1: all data is legal
  if ( first_index == first_unassembled_index_ ) {
    string data_to_push = data;
    try_push_assembled_bytes(unassembled_bytes, unassembled_present,
                              data_to_push, writer, first_unassembled_index_);
    try_close(eof_received_, eof_index_, writer);
    return;
  }

  // case2: data starts before first_unassembled_index_
  if ( first_index < first_unassembled_index_ ){
    uint64_t skip_len = first_unassembled_index_ - first_index;
    string data_to_push = data.substr(skip_len);
    try_push_assembled_bytes(unassembled_bytes, unassembled_present,
                              data_to_push, writer, first_unassembled_index_);
    try_close(eof_received_, eof_index_, writer);
    return;
  }

  // case3: data should be stored in reassembler
  uint64_t start_index = first_index - first_unassembled_index_;
  if (first_index > first_unassembled_index_ && !data.empty()) {
    place_string_efficiently(unassembled_bytes, unassembled_present, data, start_index);
  }
}

void place_string_efficiently(
    vector<char>& container,
    vector<bool>& present,
    std::string data,
    uint64_t start_index)
{
    uint64_t data_len = data.length();
    if (start_index >= container.size()) {
        return;
    }

    uint64_t remaining_capacity = container.size() - start_index;
    uint64_t actual_len = std::min(data_len, remaining_capacity);
    if (actual_len == 0) return;

    // copy bytes (including zero bytes) and mark presence
    for (uint64_t i = 0; i < actual_len; ++i) {
        container[start_index + i] = data[i];
        present[start_index + i] = true;
    }
}

void try_close(bool recv, uint64_t idx, Writer& writer){
  if (recv and idx == writer.bytes_pushed()){
    writer.close();
  }
}

void try_push_assembled_bytes(std::vector<char>& unassembled_bytes,
                               std::vector<bool>& unassembled_present,
                               std::string& data_to_push,
                               Writer& writer,
                               uint64_t& first_unassembled_index)
{

  if (!data_to_push.empty()) {
    uint64_t pushed_len = data_to_push.length();
    writer.push(data_to_push);
    first_unassembled_index += pushed_len;
    data_to_push.clear();

    if ( pushed_len > 0 && !unassembled_bytes.empty() ) {
      uint64_t num_to_erase = std::min( pushed_len, unassembled_bytes.size() );
      unassembled_bytes.erase( unassembled_bytes.begin(), unassembled_bytes.begin() + num_to_erase );
      unassembled_present.erase( unassembled_present.begin(), unassembled_present.begin() + num_to_erase );
    }
  }

  if (unassembled_bytes.empty()) {
    return;
  }

  // find continuous data
  uint64_t it = 0;
  string additional_data;
  for (; it < unassembled_bytes.size(); it++) {
    if (unassembled_present[it]) {
      additional_data += unassembled_bytes[it];
    } else {
      break;
    }
  }

  if (it > 0) {
    if (!additional_data.empty()) {
      writer.push(additional_data);
      first_unassembled_index += additional_data.length();
    }
    
    unassembled_bytes.erase(unassembled_bytes.begin(), unassembled_bytes.begin() + it);
    unassembled_present.erase(unassembled_present.begin(), unassembled_present.begin() + it);
    
    // point to pass test9
    uint64_t new_capacity = writer.available_capacity();
    if (unassembled_bytes.size() < new_capacity) {
      unassembled_bytes.resize(new_capacity);
      unassembled_present.resize(new_capacity, false);
    }
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t cnt = 0;
  for (bool b : unassembled_present) {
    if (b) ++cnt;
  }
  return cnt;
}

```

## 基准表现:
``` 
 9/18 Test #10: reassembler_cap ..................   Passed    0.03 sec
      Start 11: reassembler_seq
10/18 Test #11: reassembler_seq ..................   Passed    1.71 sec
      Start 12: reassembler_dup
11/18 Test #12: reassembler_dup ..................   Passed    0.14 sec
      Start 13: reassembler_holes
12/18 Test #13: reassembler_holes ................   Passed    0.22 sec
      Start 14: reassembler_overlapping
13/18 Test #14: reassembler_overlapping ..........   Passed    0.05 sec
      Start 15: reassembler_win
14/18 Test #15: reassembler_win ..................   Passed   12.08 sec
      Start 37: no_skip
15/18 Test #37: no_skip ..........................   Passed    0.02 sec
      Start 38: compile with optimization
16/18 Test #38: compile with optimization ........   Passed   21.18 sec
      Start 39: byte_stream_speed_test
        ByteStream throughput (pop length 4096):  8.55 Gbit/s
        ByteStream throughput (pop length 128):   1.97 Gbit/s
        ByteStream throughput (pop length 32):    0.62 Gbit/s
17/18 Test #39: byte_stream_speed_test ...........   Passed    0.41 sec
      Start 40: reassembler_speed_test
        Reassembler throughput (no overlap):  20.37 Gbit/s
        Reassembler throughput (10x overlap):  4.13 Gbit/s
18/18 Test #40: reassembler_speed_test ...........   Passed    0.43 sec
```
其中reassembler_seq的表现非常差劲, perf结果:

```
99.48% 0.00% reassembler_seq reassembler_seq [.] _start
99.48% 0.00% reassembler_seq libc.so.6 [.] __libc_start_main@@GLIBC_2.34
99.48% 0.00% reassembler_seq libc.so.6 [.] __libc_start_call_main
99.43% 0.00% reassembler_seq reassembler_seq [.] main
99.37% 0.00% reassembler_seq reassembler_seq [.] TestHarness<Reassembler>::execute(TestStep<Reassembler> const&)
98.45% 0.00% reassembler_seq reassembler_seq [.] Insert::execute(Reassembler&) const
98.45% 0.00% reassembler_seq reassembler_seq [.] Reassembler::insert(unsigned long, std::__cxx11::basic_string<ch
98.28% 0.00% reassembler_seq reassembler_seq [.] try_push_assembled_bytes(std::vector<char, std::allocator<char>
97.94% 0.00% reassembler_seq reassembler_seq [.] std::vector<bool, std::allocator<bool> >::erase(std::Bit_const
97.94% 0.00% reassembler_seq reassembler_seq [.] std::vector<bool, std::allocator<bool> >::_M_erase(std::_Bit_ite
97.94% 0.00% reassembler_seq reassembler_seq [.] std::_Bit_iterator std::copy<std::_Bit_iterator, std::_Bit_itera
97.94% 0.00% reassembler_seq reassembler_seq [.] std::_Bit_iterator std::__copy_move_a<false, std::_Bit_iterator,
97.94% 0.00% reassembler_seq reassembler_seq [.] std::_Bit_iterator std::__copy_move_a1<false, std::_Bit_iterator
97.94% 0.00% reassembler_seq reassembler_seq [.] std::_Bit_iterator std::__copy_move_a2<false, std::_Bit_iterator
92.52% 4.46% reassembler_seq reassembler_seq [.] std::_Bit_iterator std::__copy_move<false, false, std::random_ac
43.24% 28.64% reassembler_seq reassembler_seq [.] std::_Bit_iterator::operator*() const
30.62% 4.09% reassembler_seq reassembler_seq [.] std::_Bit_reference::operator=(std::_Bit_reference const&)
16.61% 4.77% reassembler_seq reassembler_seq [.] std::_Bit_iterator::operator++()
16.38% 15.99% reassembler_seq reassembler_seq [.] std::_Bit_reference::operator bool() const
15.34% 15.12% reassembler_seq reassembler_seq [.] std::_Bit_reference::_Bit_reference(unsigned long*, unsigned lon
13.19% 13.19% reassembler_seq reassembler_seq [.] std::_Bit_iterator_base::_M_bump_up()
10.73% 10.62% reassembler_seq reassembler_seq [.] std::_Bit_reference::operator=(bool)
0.99% 0.00% reassembler_seq [kernel.kallsyms] [k] asm_sysvec_apic_timer_interrupt

```
## 分析

程序性能瓶颈在于使用了 std::vector<bool> 并且频繁在头部进行 erase 操作。98% 的 CPU 时间 都消耗在 try_push_assembled_bytes 函数中对 unassembled_present (即 std::vector<bool>) 的 erase 操作上。这导致了大量的位操作（Bit manipulation）和内存搬运，将原本应该是线性的复杂度变成了平方级复杂度 $O(N^2)$。

## 尝试解决

### 1. vector\<bool>效率太低, 尝试使用vector\<uint8_t>

表现:
```
16/18 Test #38: compile with optimization ........   Passed    0.25 sec
      Start 39: byte_stream_speed_test
        ByteStream throughput (pop length 4096):  9.09 Gbit/s
        ByteStream throughput (pop length 128):   1.86 Gbit/s
        ByteStream throughput (pop length 32):    0.59 Gbit/s
17/18 Test #39: byte_stream_speed_test ...........   Passed    0.44 sec
      Start 40: reassembler_speed_test
        Reassembler throughput (no overlap):  22.32 Gbit/s
        Reassembler throughput (10x overlap):  3.68 Gbit/s
18/18 Test #40: reassembler_speed_test ...........   Passed    0.48 sec
```
其中:
```
10/18 Test #11: reassembler_seq ..................   Passed    0.05 sec
```
reassembler_seq耗时从1.71 sec -> 0.05 sec

总结:
|    操作    | `std::vector<uint8_t>` (当前) |    `std::vector<bool>` (之前)    |
|----------|---------------------------|------------------------------|
|   **存储方式**   |      1 byte per bool      |        1 bit per bool        |
|   **内存访问**   |          直接内存地址           |       读取字 + 位运算掩码 + 位移       |
| **`erase` 实现** |   `memmove` (极快，CPU 硬件优化)   | 软件循环：读取双字 -> 位移拼接 -> 写入 (极慢) |
|  **复杂度常数**   |            极小             |        巨大 (由于位操作指令多)         |

但是吞吐量几乎没有变化, 还有进一步优化空间