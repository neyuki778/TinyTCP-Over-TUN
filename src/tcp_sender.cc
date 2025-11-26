#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include <string_view>
#include <algorithm>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return flight_numbers_length;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{ 
  if (fast_retransmit_pending_) {
    if (!outstanding_seqno_.empty()) {
      transmit(outstanding_seqno_.front());
    }
    fast_retransmit_pending_ = false;
  }

  // zero-window probe
  uint64_t effective_window = (window_size_ == 0) ? 1 : window_size_;
  // Congestion Control
  if (cwnd_ < effective_window) effective_window = cwnd_;

  while (true) {
    if (effective_window <= sequence_numbers_in_flight()) break;
    uint64_t available_window = effective_window - sequence_numbers_in_flight();
    
    TCPSenderMessage msg = make_empty_message();
    
    // SYN
    if (!syn_sent_){
      msg.SYN = true;
      syn_sent_ = true;
      available_window -= 1;
    }
    
    // RST
    if (writer().has_error()){
      msg.RST = true;
    }
    
    const uint64_t MAX_PAYLOAD = static_cast<uint64_t>(TCPConfig::MAX_PAYLOAD_SIZE);
    string_view payload_view = reader().peek();
    
    // payload
    uint64_t payload_len = std::min({
      MAX_PAYLOAD,
      available_window,
      static_cast<uint64_t>(payload_view.size())
    });
    
    // Congestion Control: Avoid Silly Window Syndrome
    // If we are limited by cwnd (not rwnd), and we can't send a full packet, wait.
    if (payload_len < MAX_PAYLOAD && payload_len < payload_view.size()) {
      if (window_size_ > 0 && cwnd_ < window_size_) {
        break;
      }
    }

    if (payload_len > 0) {
      msg.payload = string(payload_view.substr(0, payload_len));
      available_window -= payload_len;
    }
    
    // FIN only last segment has FIN
    if (writer().is_closed() && !fin_sent_ && available_window > 0 && payload_len == payload_view.size()) {
      msg.FIN = true;
      fin_sent_ = true;
    }
    
    if (msg.sequence_length() == 0) {
      break;
    }

    transmit(msg);
    outstanding_seqno_.emplace_back(msg);
    next_seqno_ += msg.sequence_length();
    flight_numbers_length += msg.sequence_length();
    is_timer_runnning_ = true;
    
    if (msg.payload.size() > 0) {
      reader().pop(msg.payload.size());
    }
    
    if (msg.FIN) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap(next_seqno_, isn_);
  msg.RST = reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.RST) writer().set_error();
  window_size_ = msg.window_size;
  // msg.ackno: the verified seqno of receiver
  if (msg.ackno){
    // receiver says that the seqno has been confirmed
    uint64_t new_ackno = msg.ackno->unwrap(isn_, next_seqno_);  
    if (new_ackno > next_seqno_) return;

    // Congestion Control & State Update
    if (new_ackno > ackno_) {
        // New ACK - Reset RTO state
        current_RTO_ms_ = initial_RTO_ms_;
        consecutive_retransmissions_ = 0;
        time_elapsed_ = 0;
        is_timer_runnning_ = true;

        uint64_t old_ackno = ackno_;
        ackno_ = new_ackno;
        consecutive_duplicate_acks_ = 0;
        
        // Slow Start
        // Don't increase cwnd for SYN ACK (0 -> 1)
        if (!(old_ackno == 0 && new_ackno == 1)) {
            uint64_t bytes_acked = new_ackno - old_ackno;
            if (cwnd_ < ssthresh_) {
                cwnd_ += bytes_acked;
            } else {
                // Congestion Avoidance
                // cwnd += MSS * MSS / cwnd
                // Handle cumulative ACKs by iterating to match the test expectation
                uint64_t mss = TCPConfig::MAX_PAYLOAD_SIZE;
                uint64_t remaining = bytes_acked;
                while (remaining >= mss) {
                    cwnd_ += (mss * mss) / cwnd_;
                    remaining -= mss;
                }
                // Handle remaining bytes (less than 1 MSS)
                if (remaining > 0) {
                    cwnd_ += (remaining * mss) / cwnd_;
                }
            }
        }
    } else if (new_ackno == ackno_) {
        // Duplicate ACK logic
        // Only count duplicate ACKs if we have outstanding data
        if (!outstanding_seqno_.empty()) {
            // Check if the window size is the same (pure duplicate ACK)
            // Some tests send window updates with same ackno, which are not dup ACKs for fast retransmit
            // But standard says: same ackno, same window, same length -> dup ack.
            // Here we simplify: if ackno is same and we have outstanding data.

            // Let's stick to basic: same ackno -> dup ack.
            consecutive_duplicate_acks_++;
            if (consecutive_duplicate_acks_ == 3) {
                // Fast Retransmit
                fast_retransmit_pending_ = true;
                // Fast Recovery
                ssthresh_ = std::max(cwnd_ / 2, static_cast<uint64_t>(TCPConfig::MAX_PAYLOAD_SIZE));
                cwnd_ = ssthresh_ + 3 * TCPConfig::MAX_PAYLOAD_SIZE;
            } else if (consecutive_duplicate_acks_ > 3) {
                // Fast Recovery continues
                cwnd_ += TCPConfig::MAX_PAYLOAD_SIZE;
            }
        }
    }

    while(!outstanding_seqno_.empty()){
      auto &it = outstanding_seqno_.front();
      // how to confirm “it”?
      if (it.seqno.unwrap(isn_, next_seqno_) + it.sequence_length() <= new_ackno){
        flight_numbers_length -= outstanding_seqno_.front().sequence_length();
        outstanding_seqno_.pop_front();
      }else{
        break;
      }
    }
    
    if (outstanding_seqno_.empty()) {
        is_timer_runnning_ = false;
        time_elapsed_ = 0;
    } else if (new_ackno > ackno_) {
        // If we have outstanding data and just received a NEW ack (partial ack),
        // we must restart the timer.
        // Note: We already reset RTO/time_elapsed_ above when new_ackno > ackno_.
        // So here we just ensure the timer is running.
        is_timer_runnning_ = true;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if (is_timer_runnning_) time_elapsed_ += ms_since_last_tick;
  // shuold retransmition -- test31
  if (time_elapsed_ >= current_RTO_ms_){
    // zero-window probe
    // transmits anyway --test32 Retx SYN until too many retransmissions
    time_elapsed_ = 0;
    transmit(outstanding_seqno_.front());
    if (window_size_ > 0){
    current_RTO_ms_ *= 2;
    consecutive_retransmissions_++;
    // consecutive_retransmissions never bigger than MAX_RETX_ATTEMPTS --test32
    if (consecutive_retransmissions_ > TCPConfig::MAX_RETX_ATTEMPTS) {
        writer().set_error();
        return;
      }
    
    // Congestion Control: Timeout
    ssthresh_ = std::max(cwnd_ / 2, static_cast<uint64_t>(TCPConfig::MAX_PAYLOAD_SIZE));
    cwnd_ = TCPConfig::MAX_PAYLOAD_SIZE;
    consecutive_duplicate_acks_ = 0;
    }
  }
}
