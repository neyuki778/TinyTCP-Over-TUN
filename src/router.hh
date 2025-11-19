#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <optional>

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};
  // Define a RouteEntry struct
  struct RouteEntry {
    uint32_t route_prefix;
    uint8_t prefix_length;
    std::optional<Address> next_hop;
    size_t interface_num;

    RouteEntry(uint32_t prefix, uint8_t len, std::optional<Address> hop, size_t num)
        : route_prefix(prefix), prefix_length(len), next_hop(hop), interface_num(num) {}
  };
  // A vec for route storage
  // Trie Node structure for LPM
  struct TrieNode {
    std::array<std::unique_ptr<TrieNode>, 2> children {};
    std::optional<RouteEntry> route_entry {};
  };

  std::unique_ptr<TrieNode> trie_root_ { std::make_unique<TrieNode>() };
};
