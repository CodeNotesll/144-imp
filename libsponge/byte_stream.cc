#include "byte_stream.hh"

#include <queue>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : buffer_capacity(capacity), input_end(false), write_num(0), read_num(0) {
    /// que = queue<char>;
}

size_t ByteStream::write(const string &data) {
    if (input_ended())
        return 0;  //
    size_t to_write = min(remaining_capacity(), data.size());
    for (size_t i = 0; i < to_write; ++i) {
        que.push(data[i]);
    }
    write_num += to_write;
    return to_write;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::queue<char> new_copy = std::move(que);
    size_t to_peek = min(len, new_copy.size());
    string ans;
    while (to_peek) {
        ans.push_back(new_copy.front());
        new_copy.pop();
        to_peek--;
    }
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t to_pop = min(len, buffer_size());
    read_num += to_pop;
    while (to_pop) {
        que.pop();
        to_pop--;
    }

    return;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans;
    size_t to_read = min(len, buffer_size());
    read_num += to_read;
    while (to_read) {
        ans.push_back(que.front());
        que.pop();
        to_read--;
    }

    return ans;
}

void ByteStream::end_input() { input_end = true; }

bool ByteStream::input_ended() const { return input_end; }

size_t ByteStream::buffer_size() const { return que.size(); }

bool ByteStream::buffer_empty() const { return que.empty(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return write_num; }  // total number of bytes writen

size_t ByteStream::bytes_read() const { return read_num; }  // total number of bytes read

size_t ByteStream::remaining_capacity() const { return buffer_capacity - que.size(); }
