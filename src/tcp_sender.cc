#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <string_view>
#include <algorithm>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t cnt = 0;
  for (auto &it : outstanding_seqno_){
    cnt += it.sequence_length();
  }
  return cnt;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t available_window = window_size_ - sequence_numbers_in_flight();
  
  if (available_window == 0) return;  // Window is full, cannot send
  
  TCPSenderMessage msg = make_empty_message();
  string_view payload_view = reader().peek();
  
  // SYN
  if (!syn_sent_){
    syn_sent_ = true; 
    msg.SYN = true;
    available_window -= 1;  // SYN occupies one sequence number
  }
  
  // RST
  if (writer().has_error()){
    msg.RST = true;
  }
  
  const uint64_t MAX_PAYLOAD = static_cast<uint64_t>(TCPConfig::MAX_PAYLOAD_SIZE);
  
  // Fill the window
  while (available_window >= 0) {
    // Calculate payload length for this transmission
    uint64_t payload_len = std::min({
      MAX_PAYLOAD,
      available_window,
      static_cast<uint64_t>(payload_view.size())
    });
    
    // If there is no data to send
    if (payload_len == 0) {
      // Check if FIN can be sent
      if (writer().is_closed() && !fin_sent_ && available_window > 0) {
        msg.FIN = true;
        fin_sent_ = true;
      } else {
        break;  // No data and cannot send FIN, exit
      }
    } else {
      // Data available to send
      msg.payload = string(payload_view.substr(0, payload_len));
      payload_view = payload_view.substr(payload_len);
      
      // Check if FIN can be piggybacked
      if (writer().is_closed() && !fin_sent_ && payload_view.empty() 
          && available_window > payload_len) {
        msg.FIN = true;
        fin_sent_ = true;
      }
    }
    
    // If the message has no content (no SYN/FIN/payload), do not send
    if (msg.sequence_length() == 0) {
      break;
    }
    
    // Send the message
    transmit(msg);
    outstanding_seqno_.emplace_back(msg);
    
    // Update state
    uint64_t seq_len = msg.sequence_length();
    next_seqno_ += seq_len;
    available_window -= seq_len;
    is_timer_runnning_ = true;
    
    // Consume data from reader
    if (msg.payload.size() > 0) {
      reader().pop(msg.payload.size());
    }
    
    // Stop if FIN was sent
    if (msg.FIN) {
      break;
    }
    
    // Prepare for the next message
    msg = make_empty_message();
  }
}


TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap(next_seqno_, isn_);
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.RST) writer().set_error();
  window_size_ = msg.window_size;
  // msg.ackno: the verified seqno of receiver
  if (msg.ackno){
    // receiver says that the seqno has been confirmed
    uint64_t new_ackno = msg.ackno->unwrap(isn_, next_seqno_);  
    if (new_ackno > next_seqno_) return;
    bool reset = false;
    while(!outstanding_seqno_.empty()){
      auto &it = outstanding_seqno_.front();
      // how to confirm “it”?
      if (it.seqno.unwrap(isn_, next_seqno_) + it.sequence_length() <= new_ackno){
        outstanding_seqno_.pop_front();
        // reset rto state
        if (!reset){
          current_RTO_ms_ = initial_RTO_ms_;
          consecutive_retransmissions_ = 0;
          is_timer_runnning_ = false;
          time_elapsed_ = 0;
          reset = true;
        }
      }else{
        // partly pushed should start timer --test32
        is_timer_runnning_ = true;
        break;
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if (is_timer_runnning_) time_elapsed_ += ms_since_last_tick;
  // shuold retransmition -- test31
  if (time_elapsed_ >= current_RTO_ms_){
    // zero-window probe
    // transmits anyway --test32 Retx SYN until too many retransmissions
    time_elapsed_ = 0;
    transmit(outstanding_seqno_.front());
    if (window_size_ > 0){
    current_RTO_ms_ *= 2;
    consecutive_retransmissions_++;
    // consecutive_retransmissions never bigger than MAX_RETX_ATTEMPTS --test32
    if (consecutive_retransmissions_ > TCPConfig::MAX_RETX_ATTEMPTS) {
        writer().set_error();
        return;
      }
    }
  }
}
