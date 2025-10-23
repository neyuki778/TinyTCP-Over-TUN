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
  // zero-window probe
  uint64_t effective_window = (window_size_ == 0) ? 1 : window_size_;
  while (true) {
    if (effective_window <= sequence_numbers_in_flight()) break;
    uint64_t available_window = effective_window - sequence_numbers_in_flight();
    
    TCPSenderMessage msg = make_empty_message();
    
    // SYN
    if (!syn_sent_){
      msg.SYN = true;
      syn_sent_ = true;
      available_window -= 1;
    }
    
    // RST
    if (writer().has_error()){
      msg.RST = true;
    }
    
    const uint64_t MAX_PAYLOAD = static_cast<uint64_t>(TCPConfig::MAX_PAYLOAD_SIZE);
    string_view payload_view = reader().peek();
    
    // payload
    uint64_t payload_len = std::min({
      MAX_PAYLOAD,
      available_window,
      static_cast<uint64_t>(payload_view.size())
    });
    
    if (payload_len > 0) {
      msg.payload = string(payload_view.substr(0, payload_len));
      available_window -= payload_len;
    }
    
    // FIN only last segment has FIN
    if (writer().is_closed() && !fin_sent_ && available_window > 0 && payload_len == payload_view.size()) {
      msg.FIN = true;
      fin_sent_ = true;
    }
    
    if (msg.sequence_length() == 0) {
      break;
    }

    transmit(msg);
    outstanding_seqno_.emplace_back(msg);
    next_seqno_ += msg.sequence_length();
    is_timer_runnning_ = true;
    
    if (msg.payload.size() > 0) {
      reader().pop(msg.payload.size());
    }
    
    if (msg.FIN) {
      break;
    }
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
