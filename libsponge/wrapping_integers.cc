#include "wrapping_integers.hh"
# include<iostream>
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t isn_raw_value = isn.raw_value();
    uint64_t k = isn_raw_value + n;
    uint64_t m = static_cast<uint64_t>(1) << 32;
    uint32_t w = k%m;
    return WrappingInt32{w};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t k = n.raw_value() - isn.raw_value();
    //cout << "k is " << k << endl;
    uint64_t m = static_cast<uint64_t>(1) << 32;

    uint32_t t = checkpoint/m;
    uint32_t remain = checkpoint%m;
    uint64_t left, right;
    if (t == 0 && remain <= k) {
        return k;
    }
    if (t == m - 1 && k <= remain) {
        return t*m + k;
    }
    if (k <= remain) {  // checkpoint = t*m + remain
        left = t*m + k;
        right = (t+1)*m + k;
    }
    else { // remain < k 
        left = (t-1)*m + k;
        right = t*m + k;
    }
    if (checkpoint - left <= right - checkpoint) 
        return left;
    else 
        return right;
}
// (isn + k)mod(2^32) = n