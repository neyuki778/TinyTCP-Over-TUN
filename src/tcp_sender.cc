#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <string_view>

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
  debug( "unimplemented consecutive_retransmissions() called" );
  return {};
}

void TCPSender::push( const TransmitFunction& transmit )
{
  TCPSenderMessage msg = make_empty_message();
  string_view payload_view = reader().peek();
  // SYN
  if (!syn_sent_){
    syn_sent_ = true; 
    msg.SYN = true;
  }
  // FIN
  if (reader().is_finished()){
    msg.FIN = true;
    fin_sent_ = true;
  }
  // RST
  if (writer().has_error()){
    msg.RST = true;
  }
  // payload
  string payload(payload_view.substr(0, window_size_ - msg.sequence_length()));
  msg.payload = payload;
  // seqno
  msg.seqno = Wrap32::wrap(next_seqno_, isn_); 
  if (msg.sequence_length()){
    next_seqno_ += msg.sequence_length();
    transmit(msg);
    outstanding_seqno_.emplace_back(msg);
    // consume payload in reader -- test27
    reader().pop(payload.size());
    // resize window-size -- test28
    window_size_ -= msg.sequence_length();
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
    while(!outstanding_seqno_.empty()){
      auto &it = outstanding_seqno_.front();
      // how to confirm “it”?
      if (it.seqno.unwrap(isn_, next_seqno_) + it.sequence_length() <= new_ackno){
        outstanding_seqno_.pop_front();
      }else{
        break;
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  debug( "unimplemented tick({}, ...) called", ms_since_last_tick );
  (void)transmit;
}
