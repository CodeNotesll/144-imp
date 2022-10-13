#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader head = seg.header();
    const string &data{seg.payload().str().begin(), seg.payload().str().end()};
    const uint64_t mod = static_cast<uint64_t>(1) << 32;

    if (head.syn) {
        has_isn = true;
        isn = head.seqno;
    }

    if (head.fin) {
        has_fin = true;
        uint32_t add = data.size();
        if (head.syn)
            add++;
        fin = WrappingInt32((head.seqno.raw_value() + add) % mod);
    }

    uint64_t absSeqno = unwrap(head.seqno, isn, _reassembler.stream_out().bytes_written());
    if (head.syn)
        _reassembler.push_substring(data, absSeqno, head.fin);
    else
        _reassembler.push_substring(data, absSeqno - 1, head.fin);
    return;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (has_isn == false) {  // syn not received
        return {};
    }
    if (has_fin && _reassembler.stream_out().input_ended()) {
        return fin + 1;  // fin seqno received, and input has ended, so return fin + 1
    }
    uint64_t stream_index = _reassembler.stream_out().bytes_written();
    WrappingInt32 ack = wrap(stream_index + 1, isn);
    return ack;
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
