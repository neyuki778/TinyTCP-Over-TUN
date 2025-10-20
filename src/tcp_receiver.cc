#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  SYN_ = message.SYN;
  FIN_ = message.FIN;
  RST_ = message.RST;

}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage msg;
  msg.RST = false;
  msg.window_size = writer().available_capacity();

  if (SYN_){
    msg.ackno = Wrap32();
  }
  if (reader().is_finished()) msg.RST = true;

}
