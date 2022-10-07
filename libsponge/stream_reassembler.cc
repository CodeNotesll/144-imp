#include "stream_reassembler.hh"

#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : unassembled_num(0), eof_index(-1), _eof(false), _output(capacity), _capacity(capacity) {}

//! \brief this fucntion sotre an string in an index-sorted map
//! the <index, string> pair in map has such compareable relation
//! _output.bytes_written() <= index_0;
//! index_(n-1) + data_(n-1).size() < _output.bytes_read() + _capacity;
//! index_i + str_i.size() < index_j; j = i + 1
//! \param put the substring
//! \param index indicates the index (place in sequence) of the first byte in `data`
void StreamReassembler::store_unassembled(const string &put, const size_t index) {
    // if (put.empty())
    //     return;
    string data = put;
    uint64_t dataleft = index;
    uint64_t dataright = dataleft + data.size();
    auto beg = mp.begin();
    while (beg != mp.end()) {
        string temp = beg->second;
        uint64_t tempL = beg->first;
        uint64_t tempR = tempL + temp.size();  // [tempL, tempR)
        if (dataright < tempL || tempR < dataleft) {
            beg++;     // [dataleft, dataright)     [tempL, tempR)
            continue;  // [tempL, tmepR)    [dataleft, dataright)
        }
        // has overlap
        beg = mp.erase(beg);
        unassembled_num -= temp.size();
        string merge;
        size_t datai = 0;
        size_t tempj = 0;
        while (datai < data.size() && tempj < temp.size()) {
            if (datai + dataleft == tempj + tempL)
                break;
            if (datai + dataleft < tempj + tempL) {
                merge.push_back(data[datai]);
                datai++;
            } else {
                merge.push_back(temp[tempj]);
                tempj++;
            }
        }
        while (datai < data.size() && tempj < temp.size()) {
            // to_puti + to_putleft == tempj + tempL
            merge.push_back(data[datai]);
            datai++;
            tempj++;
        }
        while (datai < data.size()) {
            merge.push_back(data[datai]);
            datai++;
        }
        while (tempj < temp.size()) {
            merge.push_back(temp[tempj]);
            tempj++;
        }
        dataleft = min(dataleft, tempL);
        dataright = max(dataright, tempR);
        data = merge;
    }
    mp.insert({dataleft, data});
    unassembled_num += data.size();
}

//! \brief This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
//! \param data the substring
//! \param index indicates the index (place in sequence) of the first byte in `data`
//! \param eof the last byte of `data` will be the last byte in the entire stream
void StreamReassembler::push_substring(const string &data, const uint64_t index, const bool eof) {
    // corner case: data is empty string, and eof is true;
    // in this case: program will return at the repeated string test, it won't call end_intput()
    if (_output.input_ended())  // input has ended
        return;
    if (eof) {
        eof_index = index + data.size();
    }
    size_t expectedIndex = _output.bytes_written();  // index of unwriten byte in datastream
    if (data.size() == 0 && expectedIndex == eof_index) {
        _output.end_input();
    }
    uint64_t bytes_read = _output.bytes_read();  // index of unread byte in datastream
    uint64_t unaccept_index = bytes_read + _capacity;
    if (unaccept_index <= index)  // data's index greater than right side of window(buffer), ignored
        return;

    // index < unaccept_index
    if (index > expectedIndex) {  // put data into map
        uint64_t end_index = index + data.size();
        if (end_index <= unaccept_index) {
            store_unassembled(data, index);
        } else {  //  index < unaccept_index < end_index
            uint64_t size = unaccept_index - index;
            string temp = data;
            temp.resize(size);
            store_unassembled(temp, index);
        }
    } else {  // index <= expectedIndex; put propel assembled data to datastream
        size_t i = expectedIndex - index;
        if (i >= data.size())  // repeated string
            return;
        string temp = data.substr(i);
        uint64_t size = min(unaccept_index - expectedIndex, temp.size());
        temp.resize(size);  // trim tail
        store_unassembled(temp, expectedIndex);
    }

    // after store and merge, try to write string into bytestream
    auto beg = mp.begin();
    if (beg->first != expectedIndex)
        return;
    string temp = beg->second;
    _output.write(temp);
    mp.erase(beg);
    unassembled_num -= temp.size();
    expectedIndex = _output.bytes_written();
    if (expectedIndex == eof_index)
        _output.end_input();
    return;
}

//! The number of bytes in the substrings stored but not yet reassembled
//!
//! \note If the byte at a particular index has been pushed more than once, it
//! should only be counted once for the purpose of this function.
size_t StreamReassembler::unassembled_bytes() const { return unassembled_num; }

//! \brief Is the internal state empty (other than the output stream)?
//! \returns `true` if no substrings are waiting to be assembled
bool StreamReassembler::empty() const {
    return unassembled_num == 0;  // mp.empty() && _output.bytes_written() == eof_index;
}
