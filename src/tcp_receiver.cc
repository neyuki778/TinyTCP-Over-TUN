#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  
  uint64_t first_unassembled_index = reader().bytes_popped() + reader().bytes_buffered();
  
  if ( message.SYN ){
    SYN_ = true;
    ISN_ = message.seqno;
    first_unassembled_index += 1;
  }
  
  if ( message.FIN ){
    FIN_ = true;
  }
  
  reassembler_.insert( message.seqno.unwrap(ISN_, first_unassembled_index), message.payload, FIN_ );

}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage msg;
  msg.RST = false;
  msg.window_size = writer().available_capacity();

  if (SYN_){
    uint64_t first_unassembled_index = reader().bytes_popped() + reader().bytes_buffered();
    msg.ackno = ISN_.wrap(first_unassembled_index, ISN_);
  }
  if (reader().is_finished()) msg.RST = true;

  return msg;
}
