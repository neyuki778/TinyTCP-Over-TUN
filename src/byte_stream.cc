#include "byte_stream.hh"

using namespace std;

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity ), buffer_(), total_pushed_( 0 ), total_poped_( 0 ), closed_( false )
{}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{
  // Your code here (and in each method below)
  uint64_t max_len = Writer::available_capacity();
  uint64_t data_len = data.length();

  if ( data_len <= max_len ) {
    buffer_.append( data );
    total_pushed_ += data_len;
  } else {
    buffer_.append( data, 0, max_len );
    total_pushed_ += max_len;
  }
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  closed_ = true;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  return closed_; // Your code here.
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{
  return capacity_ - ( total_pushed_ - total_poped_ ); // Your code here.
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{
  return total_pushed_; // Your code here.
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{
  string_view buffer( buffer_ );
  return buffer; // Your code here.
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  buffer_.erase( 0, len );
  total_poped_ += len;
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  return closed_ and ( total_poped_ == total_pushed_ ); // Your code here.
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  return total_pushed_ - total_poped_; // Your code here.
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  return total_poped_; // Your code here.
}
