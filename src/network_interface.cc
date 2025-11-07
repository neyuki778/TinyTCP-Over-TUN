#include <iostream>
#include<cassert>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t ip = next_hop.ipv4_numeric();
  if (arp_cache_.count(ip)){
    const ArpEntry& entry = arp_cache_.at(ip);
    EthernetAddress dst_mac = entry.mac;
    EthernetFrame e_frame;
    // capsulate InternetDatagram -> EthernetFrame and transmit it
    e_frame.header.src = ethernet_address_;
    e_frame.header.dst = dst_mac;
    e_frame.header.type = EthernetHeader::TYPE_IPv4;

    Serializer serializer;
    dgram.serialize(serializer);
    e_frame.payload = serializer.finish();

    transmit(e_frame);
  }else{
    uint32_t target_ip = next_hop.ipv4_numeric();

    if (!arp_request_times_.count(target_ip) or total_time_ms_ >= arp_request_times_.at(target_ip)){
    
      ARPMessage arp_msg;
      EthernetFrame arp_frame;
      Serializer serializer;

      // init arp msg
      arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
      arp_msg.sender_ethernet_address = ethernet_address_;
      arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
      // arp_msg.target_ethernet_address = ETHERNET_BROADCAST;
      arp_msg.target_ip_address = target_ip;
      arp_msg.serialize(serializer);
      
      // init arp ethernet frame
      arp_frame.payload = serializer.finish();
      
      arp_frame.header.src = ethernet_address_;
      arp_frame.header.dst = ETHERNET_BROADCAST;
      arp_frame.header.type = EthernetHeader::TYPE_ARP;

      transmit(arp_frame);
      arp_request_times_[target_ip] = total_time_ms_ + REQUEST_THROTTLE_MS;
    }
    pending_ip_datagrams_[target_ip].push(dgram);
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  EthernetAddress dst = frame.header.dst;
  if (dst != ethernet_address_ and dst != ETHERNET_BROADCAST){
    return;
  }

  uint16_t type = frame.header.type;
  assert((type == EthernetHeader::TYPE_ARP or type == EthernetHeader::TYPE_IPv4) && "Type error!");
  Parser paser(frame.payload);

  if (type == EthernetHeader::TYPE_IPv4){
    InternetDatagram dgram;
    dgram.parse(paser);
    datagrams_received_.push(dgram);
  }else{
    ARPMessage arp_msg;
    arp_msg.parse(paser);
    // learn
    EthernetAddress mac = arp_msg.sender_ethernet_address;
    uint32_t ip = arp_msg.sender_ip_address;
    arp_cache_[ip] = {mac, total_time_ms_ + MAPPING_TTL_MS};
    // check should send
    if (pending_ip_datagrams_.count(ip)){
      auto& pending_queue = pending_ip_datagrams_.at( ip );
      while ( !pending_queue.empty() ) {
        InternetDatagram pending_dgram = pending_queue.front();
        pending_queue.pop();
        send_datagram( pending_dgram, Address::from_ipv4_numeric( ip ) );
      }
      pending_ip_datagrams_.erase( ip ); 
    }
    // reponse if need
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST 
         && arp_msg.target_ip_address == ip_address_.ipv4_numeric() ) {
      ARPMessage reply;
      reply.opcode = ARPMessage::OPCODE_REPLY;
      reply.sender_ethernet_address = ethernet_address_;
      reply.sender_ip_address = ip_address_.ipv4_numeric();
      reply.target_ethernet_address = arp_msg.sender_ethernet_address;
      reply.target_ip_address = arp_msg.sender_ip_address;
      
      EthernetFrame reply_frame;
      reply_frame.header.src = ethernet_address_;
      reply_frame.header.dst = arp_msg.sender_ethernet_address;
      reply_frame.header.type = EthernetHeader::TYPE_ARP;
      
      Serializer serializer;
      reply.serialize( serializer );
      reply_frame.payload = serializer.finish();
      
      transmit( reply_frame );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  total_time_ms_ += ms_since_last_tick;
  // delete expired mapping pair
  for (auto it = arp_cache_.begin(); it != arp_cache_.end();){
    const ArpEntry& entry = it->second;
    if (total_time_ms_ >= entry.expiration_time_ms){
      it = arp_cache_.erase(it);
    }else{
      ++it;
    }
  }
}