#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_header.hh"
#include "tcp_segment.hh"
#include "tcp_state.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;


//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , alive_time(0)
    , wait_time(retx_timeout)
    , consecutive_retransmission_num(0)
    , syn_sent(false)
    , fin_sent(false)
    , rwnd(1)
    , LastByteSent(0)
    , LastByteAcked(0){}

uint64_t TCPSender::bytes_in_flight() const { 
    return LastByteSent - LastByteAcked; 
}

void TCPSender::fill_window() {
    if (_stream.buffer_empty() && syn_sent == true && !_stream.input_ended()) {
        //cout << "first " << endl;
        return;  // buffer is empty and syn has been sent, but input stream not reach ending
    }
    if (_stream.input_ended() && fin_sent == true) {
        //cout << "second" << endl;
        return;  // input ended and fin is sent; no data to send,
                 // and even fin not acked, tick is responsible to retransmit
    }
    //cout << "fill_window() called" << endl;
    uint16_t _rwnd = rwnd ? rwnd : 1;
    while (LastByteSent - LastByteAcked < _rwnd) {
        TCPSegment seg;
        if (syn_sent == false) {
            seg.header().syn = true;
            syn_sent = true;
        }
        size_t room = _rwnd - (LastByteSent - LastByteAcked); 
        size_t to_send_size = min(room, TCPConfig::MAX_PAYLOAD_SIZE);
        Buffer buf(_stream.read(to_send_size));
        seg.payload() = buf;
        seg.header().seqno = next_seqno();
        if (_stream.input_ended() && _stream.buffer_empty() && fin_sent == false) {
            if (seg.length_in_sequence_space() + 1 <= room) {
                seg.header().fin = true;
                fin_sent = true;
            }
        }
        size_t seq_len = seg.length_in_sequence_space();
        //cout << "seq_len is " << seq_len << endl;
        if (!seq_len) {
            break;
        }
        _next_seqno += seq_len;
        LastByteSent += seq_len;
        _segments_out.push(seg);
        _outstanding_seg.push(seg);  // store not acked seg in an internal data stucture
        if (alarmer.isstop()) {
            alarmer.run(alive_time, wait_time);
        }
    }
    return;
}

//! \param ackno the remote receiver's ackno (acknowledgment number)
//! \param window_size the remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (ackno.raw_value() > next_seqno().raw_value()) {
        return;  // ignore an impossiable ackno;
    }

    rwnd = window_size;  
    wait_time = _initial_retransmission_timeout;  // 收到了ack，重置拥塞控制信息
    consecutive_retransmission_num = 0;

    while (!_outstanding_seg.empty()) {
        TCPSegment seg = _outstanding_seg.front();
        int32_t dist = ackno - seg.header().seqno;
        int32_t seqo_space = seg.length_in_sequence_space();
        if (dist >= seqo_space) {
            _outstanding_seg.pop();  // this segment is fully acknowedged
            LastByteAcked += seg.length_in_sequence_space();
            alarmer.stop();
        } else {
            break;
        }
    }

    if (_outstanding_seg.empty()) {
        alarmer.stop();
    } else if (alarmer.isstop()) {  // _outstanding_seg.empty() is not empty() and alarmer is stop, restart it
        alarmer.run(alive_time, wait_time);
    }
    return;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    alive_time += ms_since_last_tick;
    if (alive_time < alarmer.end_time() || alarmer.isstop()) {
        return;   // not time out or no outstanding segment
    }

    // alive_time >= alarmer.end_time() && alarmer.isstop() == false
    // timeout and alarmer is running (outstanding segment)
    TCPSegment seg = _outstanding_seg.front();
    _segments_out.push(seg);
    if (rwnd) {
       wait_time *= 2;
       consecutive_retransmission_num++;
    }  

    alarmer.run(alive_time, wait_time);
}

unsigned int TCPSender::consecutive_retransmissions() const { return consecutive_retransmission_num; }

void TCPSender::send_empty_segment() {
    TCPHeader head;
    TCPSegment seg;
    // syn and fin flag default to be false
    head.seqno = next_seqno();
    seg.header() = head;
    seg.payload() = Buffer();   // empty payload

    _segments_out.push(seg);
    return;
}
