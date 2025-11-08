#include "router.hh"
#include "debug.hh"

#include <iostream>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  routing_table_.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for (shared_ptr<NetworkInterface>& iface : interfaces_){
    auto& dgram_queue = iface->datagrams_received();
    while (!dgram_queue.empty()){
      InternetDatagram dgram = move(dgram_queue.front());
      dgram_queue.pop();
      dgram.header.ttl --;
      // check TTL
      if (dgram.header.ttl <= 0){
        continue;
      }
      // match the routing rule with the highest matching degree
      // drop dgram if no matching routing rule there
      uint16_t matching_routing_max_len = 0;
      uint32_t msg_dst_ip = dgram.header.dst;
      int matching_routing_num = -1;
      int i = 0;
      // longest-prefix match
      for (auto& route : routing_table_){
        uint8_t& prefix_len = route.prefix_length;
        uint32_t bit_mask = (prefix_len == 0) ? 0: (~0U << (32 - prefix_len));
        if ((bit_mask & route.route_prefix) == (bit_mask & msg_dst_ip) and static_cast<uint16_t>(prefix_len) > matching_routing_max_len){
          matching_routing_max_len = prefix_len;
          matching_routing_num = i;
        }
        i++;
      }
      // check matching result, send if matched
      if (matching_routing_num >= 0 and matching_routing_max_len > 0){
        RouteEntry& matching_routing_rule = routing_table_.at(matching_routing_num);
        shared_ptr<NetworkInterface> target_iface = interface(matching_routing_rule.interface_num);
        Address addr = Address::from_ipv4_numeric(msg_dst_ip);
        // if next has not value, this router is the last router
        if (matching_routing_rule.next_hop.has_value()){
          addr = matching_routing_rule.next_hop.value();
        }
        target_iface->send_datagram(dgram, addr);
      }
    }
  }
}
