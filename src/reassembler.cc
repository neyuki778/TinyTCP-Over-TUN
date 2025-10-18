#include "reassembler.hh"
#include "debug.hh"
#include<string_view>
#include<vector>
#include<algorithm>

using namespace std;

void place_string_efficiently(std::vector<char>& container, std::string_view data, uint64_t start_index);

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // debug( "unimplemented insert({}, {}, {}) called", first_index, data, is_last_substring );
  Writer writer = output_.writer();
  Reader reader = output_.reader();

  uint64_t buffered_in_byte_stream = reader.bytes_buffered(); 
  uint64_t popped_bytes_index = reader.bytes_popped();
  uint64_t available_capacity = writer.available_capacity();
  uint64_t last_index = first_index + data.length();

  first_unpopped_index_ = popped_bytes_index;
  first_unassembled_index_ = first_unpopped_index_ + buffered_in_byte_stream;
  first_unacceptable_index_ = first_unassembled_index_ + available_capacity;

  //case0: stream index is too big
  if ( first_index > first_unacceptable_index_) return;

  // case1: all data is legal
  if ( first_index == first_unassembled_index_ ) {
    // writer.push( data );
    // first_unpopped_index_ += data.length();
    if (!unassembled_bytes.empty()){
      uint64_t first_index_in_unassembled_bytes = last_index - first_unassembled_index_;
      auto it = first_index_in_unassembled_bytes;
      for (; it < unassembled_bytes.size(); it++){
        if (unassembled_bytes[it] != '\0'){
          data += unassembled_bytes[it];
        }else{
          break;
        }
      }
      unassembled_bytes.erase(unassembled_bytes.begin(), unassembled_bytes.begin() + it);
    }
    writer.push(data);
  }

  // case2: data is longer than popped
  if ( first_index < first_unassembled_index_ ){
    writer.push( data.substr(first_unassembled_index_ - first_index ));
  }

  // case3: data should be stored in ressembler, suggested that first_index > first_unassembled_index_
  uint64_t start_index = first_index - first_unassembled_index_;
  if (unassembled_bytes.empty()) unassembled_bytes.resize(available_capacity, '\0'); 
  place_string_efficiently(unassembled_bytes, data, start_index);

  // close after push last substring
  if (is_last_substring) {
    output_.writer().close();
  }
}

void place_string_efficiently(
    vector<char>& container, 
    std::string_view data, 
    uint64_t start_index)
{
    uint64_t data_len = data.length();
    if (start_index >= container.size()) {
        return;
    }
    
    uint64_t remaining_capacity = container.size() - start_index;
    uint64_t actual_len = std::min(data_len, remaining_capacity);
    if (actual_len == 0) return;

    std::copy_n(
        data.data(),            // 源数据的起始指针
        actual_len,               // 复制的长度
        &container[start_index] // 目标地址的起始指针
    );
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  debug( "unimplemented count_bytes_pending() called" );
  return {};
}
