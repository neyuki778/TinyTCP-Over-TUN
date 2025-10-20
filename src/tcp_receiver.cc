#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  debug( "unimplemented receive() called" );
  (void)message;
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage msg;
  msg.RST = false;
  msg.window_size = writer().available_capacity();

  if (received_ISN_){
    msg.ackno = Wrap32();
  }
  if (reader().is_finished()) msg.RST = true;

}
