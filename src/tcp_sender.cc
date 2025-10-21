#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <string_view>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  debug( "unimplemented sequence_numbers_in_flight() called" );
  return {};
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
  // 
  if (!window_size_) {
    transmit(msg);
    return;
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
  msg.seqno = Wrap32::wrap(next_ackno_, isn_); 
  next_ackno_ += msg.sequence_length();
  transmit(msg);
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  // next_ackno_ ?
  ackno_ = msg.ackno->unwrap(isn_, next_ackno_);
  if (msg.RST) writer().set_error();
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  debug( "unimplemented tick({}, ...) called", ms_since_last_tick );
  (void)transmit;
}
