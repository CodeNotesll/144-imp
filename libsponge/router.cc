#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    if (prefix_length > 32) {
        throw runtime_error("error: prfix_length > 32");
        return;
    }
    auto t = std::make_tuple(route_prefix, prefix_length, next_hop,interface_num);
    route_table.push_back(t);
    return;
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    if (dgram.header().ttl <= 1)  // drop the datagram 
        return; 
    uint32_t dst = dgram.header().dst;  // shift right and shift left 
    int max_match_length = -1;
    int index = -1;
    int n = route_table.size();
    int de_index = -1;
    for (int i = 0; i < n; ++i) {
        uint32_t prefix = std::get<0>(route_table[i]);
        uint8_t prefix_length = std::get<1>(route_table[i]);
        if (prefix >> (32 - prefix_length) == dst >> (32 - prefix_length)) {
            if (int(prefix_length) > max_match_length) {
                max_match_length = int(prefix_length);
                index = i;
            }
        }
        if (prefix == 0)
            de_index = i;
    } 
    if (index == -1 && de_index == -1) {  // no matching routing rule
        //return; 
    }
    index = index == -1 ? de_index : index;
    std::optional<Address> next_hop = std::get<2>(route_table[index]);
    size_t num = std::get<3>(route_table[index]);
    dgram.header().ttl --;
    if (next_hop.has_value()) {
        interface(num).send_datagram(dgram, next_hop.value());
    }
    else {
        interface(num).send_datagram(dgram, Address::from_ipv4_numeric(dst)); 
    }
    return ;
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
