#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame to_send{};

    if (addr_mp.count(next_hop_ip)) {  // send an ipv4 datagram
        to_send.header().type = EthernetHeader::TYPE_IPv4;
        to_send.header().dst = addr_mp[next_hop_ip];
        to_send.header().src = _ethernet_address;
        to_send.payload() = dgram.serialize();
    } else {  // destination ethernetaddr unknown; broadcast an arp
        if (request_mp.count(next_hop_ip) && cur_time < request_mp[next_hop_ip] + request_wait_time) {
            kept_ip_datagram[next_hop_ip].push(dgram);  // queue the datagram 
            return;
        }
        to_send.header().type = EthernetHeader::TYPE_ARP;
        to_send.header().dst = ETHERNET_BROADCAST;  //
        to_send.header().src = _ethernet_address;

        ARPMessage arp;
        arp.opcode = ARPMessage::OPCODE_REQUEST;
        arp.sender_ethernet_address = _ethernet_address;
        arp.sender_ip_address = _ip_address.ipv4_numeric();
        // arp.target_ethernet_address = ;  target_ethernet_address is what requested
        arp.target_ip_address = next_hop_ip;

        to_send.payload() = BufferList(arp.serialize());  // arp message as ethernet payload
        request_mp[next_hop_ip] = cur_time;         // remember request time
        kept_ip_datagram[next_hop_ip].push(dgram);  // remember dgram
    }

    _frames_out.push(to_send);
    return;
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        if (frame.header().dst != _ethernet_address)  // ipv4 datagram received,
            return {};
        InternetDatagram ret;
        if (ret.parse(Buffer(frame.payload())) == ParseResult::NoError) {
            return ret;
        }
        throw runtime_error("Internetdatagram parser error");
        return {};
    }

    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        if (arp.parse(Buffer(frame.payload())) != ParseResult::NoError) {
            throw runtime_error("Arp message parser error");
            return {};
        }
        // learn sender's mappings from both requests and replies
        addr_mp[arp.sender_ip_address] = arp.sender_ethernet_address;
        kept_time_ip[cur_time] = arp.sender_ip_address;

        // requested arq message got reply
        if (arp.opcode == ARPMessage::OPCODE_REPLY && arp.target_ip_address == _ip_address.ipv4_numeric()) {
            request_mp.erase(arp.sender_ip_address);
            while (kept_ip_datagram[arp.sender_ip_address].empty() == false) {
                InternetDatagram datagram = kept_ip_datagram[arp.sender_ip_address].front();
                kept_ip_datagram[arp.sender_ip_address].pop();
                send_datagram(datagram, Address::from_ipv4_numeric(arp.sender_ip_address));
            }
            kept_ip_datagram.erase(arp.sender_ip_address);
        }
        // an ARP request asking for our IP address
        if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == _ip_address.ipv4_numeric()) {
            EthernetFrame to_send;
            to_send.header().type = EthernetHeader::TYPE_ARP;
            to_send.header().dst = frame.header().src;  //
            to_send.header().src = _ethernet_address;

            ARPMessage reply;
            reply.opcode = ARPMessage::OPCODE_REPLY;
            reply.sender_ethernet_address = _ethernet_address;
            reply.sender_ip_address = _ip_address.ipv4_numeric();
            reply.target_ethernet_address = arp.sender_ethernet_address;
            reply.target_ip_address = arp.sender_ip_address;

            to_send.payload() = BufferList(reply.serialize());  // arp message as ethernet payload

            _frames_out.push(to_send);
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    cur_time += ms_since_last_tick;
    // erase IP-to-Ethernet Mapping that have expired
    auto it = kept_time_ip.begin();
    while (it != kept_time_ip.end()) {
        const auto& [time, ip] = *it;
        if (time + map_keep_time > cur_time) {
            break;
        } else {  // time + map_keep_time <= cur_time
            addr_mp.erase(ip);
            it = kept_time_ip.erase(it);  // it points to next pair
        }
    }
    return;
}
