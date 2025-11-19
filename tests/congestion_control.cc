#include "sender_test_harness.hh"
#include "tcp_config.hh"
#include "wrapping_integers.hh"
#include "tcp_sender_message.hh"
#include "tcp_receiver_message.hh"
#include "random.hh"
#include <iostream>
#include <string>

using namespace std;

int main() {
    try {
        auto rd = get_random_engine();

        // Test 1: Slow Start
        {
            TCPConfig cfg;
            const Wrap32 isn(rd());
            cfg.isn = isn;

            TCPSenderTestHarness test{"Slow Start", cfg};
            
            // Start connection
            test.execute(Push{});
            test.execute(ExpectMessage{}.with_syn(true).with_seqno(isn).with_payload_size(0));
            test.execute(AckReceived{Wrap32{isn + 1}}.with_win(64000)); // Large window

            // cwnd should be 1 MSS (1000)
            // Push 2000 bytes
            test.execute(Push{string(2000, 'a')});
            
            // Should send 1000 bytes
            test.execute(ExpectMessage{}.with_payload_size(1000));
            test.execute(ExpectNoSegment{}); // cwnd full

            // Ack 1000 bytes. cwnd -> 2000
            test.execute(AckReceived{Wrap32{isn + 1 + 1000}}.with_win(64000));
            
            // Should send remaining 1000 bytes
            test.execute(ExpectMessage{}.with_payload_size(1000));
            test.execute(ExpectNoSegment{});
        }

        // Test 2: Congestion Avoidance after Timeout
        {
             TCPConfig cfg;
             const Wrap32 isn(rd());
             cfg.isn = isn;
             TCPSenderTestHarness test{"Congestion Avoidance after Timeout", cfg};

             test.execute(Push{});
             test.execute(ExpectMessage{}.with_syn(true));
             test.execute(AckReceived{Wrap32{isn + 1}}.with_win(64000));

             // cwnd = 1000.
             // Push a lot
             test.execute(Push{string(10000, 'a')});
             test.execute(ExpectMessage{}.with_payload_size(1000)); // seqno: isn+1
             
             // Ack it. cwnd = 2000.
             test.execute(AckReceived{Wrap32{isn + 1 + 1000}}.with_win(64000));
             test.execute(ExpectMessage{}.with_payload_size(1000)); // seqno: isn+1001
             test.execute(ExpectMessage{}.with_payload_size(1000)); // seqno: isn+2001
             
             // Ack 2000 more. cwnd = 4000.
             test.execute(AckReceived{Wrap32{isn + 1 + 3000}}.with_win(64000));
             test.execute(ExpectMessage{}.with_payload_size(1000)); // seqno: isn+3001
             test.execute(ExpectMessage{}.with_payload_size(1000)); // seqno: isn+4001
             test.execute(ExpectMessage{}.with_payload_size(1000)); // seqno: isn+5001
             test.execute(ExpectMessage{}.with_payload_size(1000)); // seqno: isn+6001
             
             // Now cwnd is 4000. In flight: 4000.
             // Let's timeout.
             test.execute(Tick{TCPConfig::TIMEOUT_DFLT});
             test.execute(ExpectMessage{}.with_payload_size(1000)); // Retransmit seqno: isn+3001
             
             // ssthresh should be max(4000/2, 2000) = 2000.
             // cwnd should be 1000.
             
             // Ack the retransmission. cwnd -> 2000.
             // Note: This ack acknowledges up to 4000 (retransmitted packet).
             // But we have outstanding 4001, 5001, 6001.
             test.execute(AckReceived{Wrap32{isn + 1 + 4000}}.with_win(64000));
             
             // Now cwnd is 2000. ssthresh is 2000.
             // We are at ssthresh. Next ack should be CA.
             
             // Let's Ack 7000 (everything).
             // This acknowledges 3000 bytes (4001-7000).
             // cwnd growth:
             // 1st 1000: cwnd += 1000*1000/2000 = 500. cwnd = 2500.
             // 2nd 1000: cwnd += 1000*1000/2500 = 400. cwnd = 2900.
             // 3rd 1000: cwnd += 1000*1000/2900 = 344. cwnd = 3244.
             
             test.execute(AckReceived{Wrap32{isn + 1 + 7000}}.with_win(64000));
             
             // Try to send 4000 bytes. Should only send 3 (3000 bytes) because cwnd ~3244.
             test.execute(Push{string(4000, 'b')});
             test.execute(ExpectMessage{}.with_payload_size(1000));
             test.execute(ExpectMessage{}.with_payload_size(1000));
             test.execute(ExpectMessage{}.with_payload_size(1000));
             test.execute(ExpectNoSegment{}); // blocked by cwnd ~3244
        }

    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return 1;
    }
    return EXIT_SUCCESS;
}
