#include <iostream>

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
    // capsulation InternetDatagram -> EthernetFrame and transmit it
    e_frame.header.src = ethernet_address_;
    e_frame.header.dst = dst_mac;
    e_frame.header.type = EthernetHeader::TYPE_IPv4;

    Serializer serializer;
    dgram.serialize(serializer);
    e_frame.payload = serializer.finish();

    transmit(e_frame);
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  debug( "unimplemented recv_frame called" );
  (void)frame;
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