#include "router.hh"
#include "network_interface_test_harness.hh"

#include <chrono>
#include <iostream>
#include <random>
#include <vector>

using namespace std;

// A dummy output port that does nothing, to minimize overhead during send
class BenchmarkOutputPort : public NetworkInterface::OutputPort
{
public:
  void transmit( const NetworkInterface&, const EthernetFrame& ) override {}
};

uint32_t random_ip()
{
  static std::mt19937 rng( 1337 );
  return rng();
}

int main()
{
  try {
    Router router;
    const size_t num_interfaces = 4;
    vector<shared_ptr<NetworkInterface>> interfaces;

    // 1. Setup Interfaces
    for ( size_t i = 0; i < num_interfaces; ++i ) {
      auto output_port = make_shared<BenchmarkOutputPort>();
      auto interface = make_shared<NetworkInterface>(
        "eth" + to_string( i ), output_port, EthernetAddress {}, Address { "10.0." + to_string( i ) + ".1" } );
      interfaces.push_back( interface );
      router.add_interface( interface );
    }

    // 2. Setup Routes (LPM Table)
    // Add a mix of routes with different prefix lengths
    const size_t num_routes = 10000;
    std::mt19937 rng( 42 );
    std::uniform_int_distribution<uint32_t> ip_dist;
    std::uniform_int_distribution<int> len_dist( 1, 32 );
    std::uniform_int_distribution<size_t> iface_dist( 0, num_interfaces - 1 );

    for ( size_t i = 0; i < num_routes; ++i ) {
      uint32_t ip_addr = ip_dist( rng );
      uint8_t len = len_dist( rng );
      // Ensure we don't mask out all bits if len is small, but for random IPs it's fine.
      // Actually, we should mask the IP to match the length for the route prefix.
      uint32_t mask = ( len == 0 ) ? 0 : ( ~0U << ( 32 - len ) );
      uint32_t prefix = ip_addr & mask;

      router.add_route( prefix, len, {}, iface_dist( rng ) );
    }
    // Add default route
    router.add_route( 0, 0, {}, 0 );

    // 3. Prepare Traffic
    const size_t num_packets = 100000;
    cout << "Generating " << num_packets << " packets..." << endl;

    // We will push packets directly to the receive queue of the first interface
    // to simulate incoming traffic.
    auto& input_queue = interfaces[0]->datagrams_received();

    for ( size_t i = 0; i < num_packets; ++i ) {
      InternetDatagram dgram;
      dgram.header.src = random_ip();
      dgram.header.dst = random_ip();
      dgram.header.ttl = 64;
      dgram.payload.emplace_back( string( 10, 'x' ) ); // Small payload
      dgram.header.len = dgram.header.hlen * 4 + dgram.payload.back()->size();
      
      input_queue.push( move( dgram ) );
    }

    // 4. Run Benchmark
    cout << "Starting benchmark (Route " << num_packets << " packets against " << num_routes << " routes)..." << endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    router.route();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>( end_time - start_time ).count();
    double duration_s = duration_ns / 1e9;

    cout << "Done." << endl;
    cout << "Time taken: " << duration_s << " seconds" << endl;
    cout << "Throughput: " << ( num_packets / duration_s ) << " packets/sec" << endl;
    cout << "Average time per packet: " << ( duration_ns / num_packets ) << " ns" << endl;

  } catch ( const exception& e ) {
    cerr << "Error: " << e.what() << endl;
    return 1;
  }

  return 0;
}
