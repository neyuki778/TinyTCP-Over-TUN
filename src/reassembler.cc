#include "reassembler.hh"
#include "debug.hh"
#include<string_view>
#include<vector>
#include<algorithm>

using namespace std;

void place_string_efficiently(std::vector<char>& container,
                              std::vector<bool>& present,
                              std::string_view data,
                              uint64_t start_index);

void try_close(bool recv, uint64_t idx, Writer& writer);

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // debug( "unimplemented insert({}, {}, {}) called", first_index, data, is_last_substring );
  Writer& writer = output_.writer();
  Reader& reader = output_.reader();

  uint64_t buffered_in_byte_stream = reader.bytes_buffered(); 
  uint64_t popped_bytes_index = reader.bytes_popped();
  uint64_t available_capacity = writer.available_capacity();
  uint64_t last_index = first_index + data.length();

  first_unpopped_index_ = popped_bytes_index;
  first_unassembled_index_ = first_unpopped_index_ + buffered_in_byte_stream;
  first_unacceptable_index_ = first_unassembled_index_ + available_capacity;

  // close after push last substring
  if (is_last_substring) {
    // output_.writer().close();
    eof_index_ = last_index;
    eof_received_ = true;
  }

  //case0: stream index is too big
  if ( first_index > first_unacceptable_index_) return;

  // bool pushed = false;

  // case1: all data is legal
  if ( first_index == first_unassembled_index_ ) {
    // writer.push( data );
    // first_unpopped_index_ += data.length();
    if (!unassembled_bytes.empty()){
      uint64_t first_index_in_unassembled_bytes = last_index - first_unassembled_index_;
      auto it = first_index_in_unassembled_bytes;
      for (; it < unassembled_bytes.size(); it++){
        if (unassembled_present[it]){
          data += unassembled_bytes[it];
        }else{
          break;
        }
      }
      unassembled_bytes.erase(unassembled_bytes.begin(), unassembled_bytes.begin() + it);
      unassembled_present.erase(unassembled_present.begin(), unassembled_present.begin() + it);
    }
    writer.push(data);
    // pushed = true;
    try_close(eof_received_, eof_index_, writer);
  }

  // case2: data is longer than popped
  if ( first_index < first_unassembled_index_ ){
    writer.push( data.substr(first_unassembled_index_ - first_index ));
    // pushed = true;
    try_close(eof_received_, eof_index_, writer);
  }

  // case3: data should be stored in ressembler, suggested that first_index > first_unassembled_index_
  uint64_t start_index = first_index - first_unassembled_index_;
  if (unassembled_bytes.empty()) unassembled_bytes.resize(available_capacity, '\0'); 
  // if (!pushed){
  place_string_efficiently(unassembled_bytes, unassembled_present, data, start_index);
  // }

}

void place_string_efficiently(
    vector<char>& container,
    vector<bool>& present,
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

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t cnt = 0;
  for (char c : unassembled_bytes) {
    if (c != '\0') ++cnt;
  }
  return cnt;
}
