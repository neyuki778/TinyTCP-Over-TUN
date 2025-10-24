#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
    
  if ( message.SYN ) {
    if ( !SYN_ ) {
      SYN_ = true;
      ISN_ = message.seqno;
    }
  }

  if (message.RST) {
    RST_ = true;
    reader().set_error();
    return; // Receiving RST should abort processing of the segment.
  }

  // must have seen SYN to proceed
  if ( !SYN_ ) {
    return;
  }
  
  if ( message.FIN ){
    FIN_ = true;
  }

  uint64_t checkpoint = writer().bytes_pushed() + 1;
  // balck magic
  uint64_t abs_seqno = message.seqno.unwrap( ISN_, checkpoint );

  uint64_t first_index = message.SYN ? 0 : abs_seqno - 1;
  
  reassembler_.insert( first_index, message.payload, message.FIN );

}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage msg;
  if (writer().has_error()) {
    msg.RST = true;
  } else {
    msg.RST = RST_;
  }
  msg.window_size = writer().available_capacity() > UINT16_MAX ? UINT16_MAX : writer().available_capacity();

  if (SYN_){
    // uint64_t first_unassembled_index = reader().bytes_popped() + reader().bytes_buffered();
    uint64_t first_unassembled_index = writer().bytes_pushed();
    uint64_t ackno_abs = first_unassembled_index + 1;
    if (writer().is_closed()) ackno_abs++;
    msg.ackno = Wrap32::wrap(ackno_abs, ISN_);
  }

  return msg;
}
