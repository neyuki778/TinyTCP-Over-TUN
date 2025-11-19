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
  TrieNode* current = trie_root_.get();
  for ( uint8_t i = 0; i < prefix_length; ++i ) {
    // Extract the bit at the current position (from MSB to LSB)
    // 31 - i gives the shift amount to get the bit at index i (0-indexed from MSB)
    uint8_t bit = ( route_prefix >> ( 31 - i ) ) & 1;

    if ( !current->children[bit] ) {
      current->children[bit] = make_unique<TrieNode>();
    }
    current = current->children[bit].get();
  }
  // Store the route at the node corresponding to the prefix
  // If a route already exists for this exact prefix, we overwrite it (standard behavior)
  current->route_entry = RouteEntry( route_prefix, prefix_length, next_hop, interface_num );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( shared_ptr<NetworkInterface>& iface : interfaces_ ) {
    auto& dgram_queue = iface->datagrams_received();
    while ( !dgram_queue.empty() ) {
      InternetDatagram dgram = move( dgram_queue.front() );
      dgram_queue.pop();

      // check TTL
      if ( dgram.header.ttl <= 1 ) {
        continue;
      }
      dgram.header.ttl--;

      // LPM using Trie
      uint32_t dst_ip = dgram.header.dst;
      TrieNode* current = trie_root_.get();
      const RouteEntry* best_match = nullptr;

      // Check for default route (root node)
      if ( current->route_entry.has_value() ) {
        best_match = &current->route_entry.value();
      }

      for ( int i = 0; i < 32; ++i ) {
        uint8_t bit = ( dst_ip >> ( 31 - i ) ) & 1;
        if ( !current->children[bit] ) {
          break; // No path continues
        }
        current = current->children[bit].get();
        if ( current->route_entry.has_value() ) {
          best_match = &current->route_entry.value();
        }
      }

      // check matching result, send if matched
      if ( best_match ) {
        shared_ptr<NetworkInterface> target_iface = interface( best_match->interface_num );
        Address addr = Address::from_ipv4_numeric( dst_ip );
        // if next has not value, this router is the last router
        if ( best_match->next_hop.has_value() ) {
          addr = best_match->next_hop.value();
        }
        target_iface->send_datagram( dgram, addr );
      }
    }
  }
}
