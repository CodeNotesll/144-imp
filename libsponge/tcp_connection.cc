#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const { 
    return alive_time - segment_received_time;
}

//void send_rst();
void TCPConnection::send_rst(){
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    _sender.send_empty_segment();
    TCPSegment _seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    if (_receiver.ackno().has_value()) {
        _seg.header().ack = true;
        _seg.header().ackno = _receiver.ackno().value();
    }
    _seg.header().win = _receiver.window_size();
    _seg.header().rst = true;
    _segments_out.push(_seg);  
    return;
}

void TCPConnection::send_all_segments() {
    while (!_sender.segments_out().empty()) {
        TCPSegment _seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            _seg.header().ack = true;
            _seg.header().ackno = _receiver.ackno().value();
        }
        _seg.header().win = _receiver.window_size();
        _segments_out.push(_seg);
    }
    return;
}
void TCPConnection::segment_received(const TCPSegment &seg) { 
    segment_received_time = alive_time;
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return ;
    }
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    _receiver.segment_received(seg);
    if (!_receiver.ackno().has_value()) // segment received, but _receiver.ackno() has no value 
        return;                       
    size_t before = _sender.segments_out().size();
    _sender.fill_window();
    size_t after = _sender.segments_out().size();
    if (seg.length_in_sequence_space() && after == before) {
        _sender.send_empty_segment(); 
    } 
    else if (seg.length_in_sequence_space() == 0 && _receiver.ackno().has_value() and 
            seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment(); 
    } 
    // send_all_segments()
    send_all_segments();

    if (_receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended()
        && _sender.has_fin_sent() == false) {
        _linger_after_streams_finish = false;
    } 
    return ;
}

bool TCPConnection::active() const {
    if (_sender.stream_in().error() && _receiver.stream_out().error())
        return false;
    // Prereq #1 The inbound stream has been fully assembled and has ended
    if (!(_receiver.unassembled_bytes() == 0 and _receiver.stream_out().input_ended()))
        return true; // Prereq #1 not statisfied, still active 

    // Prereq #2 The outbound stream has been ended by the local application and fully sent 
    // (including the fact that it ended, i.e. a segment with fin ) to the remote peer
    if (!(_sender.stream_in().input_ended() and _sender.has_fin_sent())) 
        return true; // Prereq #2 not statisfied, still active
    
    // Prereq #3 The outbound stream has been fully acknowledged by the remote peer.
    if (_sender.bytes_in_flight())
        return true; // Prereq #3 not statisfied, still active
    
    // Prereq# 1,2,3 satisfied 
    if (_linger_after_streams_finish == false) {
        return false;
    }
    return time_since_last_segment_received() < 10*_cfg.rt_timeout;
}

size_t TCPConnection::write(const string &data) {
    size_t n = _sender.stream_in().write(data);
    _sender.fill_window();

    // send_all_segments()
    send_all_segments(); 
    return n;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    alive_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // send_rst()
        send_rst();
        return;
    }
    // send_all_segments()
    send_all_segments();
    return;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_all_segments(); 
    return;
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_all_segments();
    return;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // send_rst()
            // Your code here: need to send a RST segment to the peer
            send_rst(); 
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
