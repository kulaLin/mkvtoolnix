/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   A class for file-like access on the bit level

   The bit_reader_c class was originally written by Peter Niemayer
     <niemayer@isg.de> and modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_BIT_CURSOR_H
#define MTX_COMMON_BIT_CURSOR_H

#include "common/common_pch.h"

#include "common/mm_io_x.h"

class bit_reader_c {
private:
  const unsigned char *m_end_of_data;
  const unsigned char *m_byte_position;
  const unsigned char *m_start_of_data;
  std::size_t m_bits_valid;
  bool m_out_of_data;

public:
  bit_reader_c(unsigned char const *data, std::size_t len) {
    init(data, len);
  }

  void init(const unsigned char *data, std::size_t len) {
    m_end_of_data   = data + len;
    m_byte_position = data;
    m_start_of_data = data;
    m_bits_valid    = len ? 8 : 0;
    m_out_of_data   = m_byte_position >= m_end_of_data;
  }

  bool eof() {
    return m_out_of_data;
  }

  uint64_t get_bits(std::size_t n) {
    uint64_t r = 0;

    while (n > 0) {
      if (m_byte_position >= m_end_of_data) {
        m_out_of_data = true;
        throw mtx::mm_io::end_of_file_x();
      }

      std::size_t b = 8; // number of bits to extract from the current byte
      if (b > n)
        b = n;
      if (b > m_bits_valid)
        b = m_bits_valid;

      std::size_t rshift = m_bits_valid - b;

      r <<= b;
      r  |= ((*m_byte_position) >> rshift) & (0xff >> (8 - b));

      m_bits_valid -= b;
      if (0 == m_bits_valid) {
        m_bits_valid     = 8;
        m_byte_position += 1;
      }

      n -= b;
    }

    return r;
  }

  inline int get_bit() {
    return get_bits(1);
  }

  inline int get_unary(bool stop,
                       int len) {
    int i;

    for (i = 0; (i < len) && get_bit() != stop; ++i)
      ;

    return i;
  }

  inline int get_012() {
    if (!get_bit())
      return 0;
    return get_bits(1) + 1;
  }

  inline uint64_t get_unsigned_golomb() {
    int n = 0;

    while (get_bit() == 0)
      ++n;

    auto bits = get_bits(n);

    return (1u << n) - 1 + bits;
  }

  inline int64_t get_signed_golomb() {
    int64_t v = get_unsigned_golomb();
    return v & 1 ? (v + 1) / 2 : -(v / 2);
  }

  uint64_t peek_bits(std::size_t n) {
    uint64_t r                             = 0;
    const unsigned char *tmp_byte_position = m_byte_position;
    std::size_t tmp_bits_valid                  = m_bits_valid;

    while (0 < n) {
      if (tmp_byte_position >= m_end_of_data)
        throw mtx::mm_io::end_of_file_x();

      std::size_t b = 8; // number of bits to extract from the current byte
      if (b > n)
        b = n;
      if (b > tmp_bits_valid)
        b = tmp_bits_valid;

      std::size_t rshift = tmp_bits_valid - b;

      r <<= b;
      r  |= ((*tmp_byte_position) >> rshift) & (0xff >> (8 - b));

      tmp_bits_valid -= b;
      if (0 == tmp_bits_valid) {
        tmp_bits_valid     = 8;
        tmp_byte_position += 1;
      }

      n -= b;
    }

    return r;
  }

  void get_bytes(unsigned char *buf, std::size_t n) {
    if (8 == m_bits_valid) {
      get_bytes_byte_aligned(buf, n);
      return;
    }

    for (auto idx = 0u; idx < n; ++idx)
      buf[idx] = get_bits(8);
  }

  void byte_align() {
    if (8 != m_bits_valid)
      skip_bits(m_bits_valid);
  }

  void set_bit_position(std::size_t pos) {
    if (pos > (static_cast<std::size_t>(m_end_of_data - m_start_of_data) * 8)) {
      m_byte_position = m_end_of_data;
      m_out_of_data   = true;

      throw mtx::mm_io::end_of_file_x();
    }

    m_byte_position = m_start_of_data + (pos / 8);
    m_bits_valid    = 8 - (pos % 8);
  }

  int get_bit_position() const {
    return (m_byte_position - m_start_of_data) * 8 + 8 - m_bits_valid;
  }

  int get_remaining_bits() const {
    return (m_end_of_data - m_byte_position) * 8 - 8 + m_bits_valid;
  }

  void skip_bits(std::size_t num) {
    set_bit_position(get_bit_position() + num);
  }

  void skip_bit() {
    set_bit_position(get_bit_position() + 1);
  }

  uint64_t skip_get_bits(std::size_t to_skip,
                         std::size_t to_get) {
    skip_bits(to_skip);
    return get_bits(to_get);
  }

protected:
  void get_bytes_byte_aligned(unsigned char *buf, std::size_t n) {
    auto bytes_to_copy = std::min<std::size_t>(n, m_end_of_data - m_byte_position);
    std::memcpy(buf, m_byte_position, bytes_to_copy);

    m_byte_position += bytes_to_copy;

    if (bytes_to_copy < n) {
      m_out_of_data = true;
      throw mtx::mm_io::end_of_file_x();
    }
  }
};
using bit_reader_cptr = std::shared_ptr<bit_reader_c>;

class bit_writer_c {
private:
  unsigned char *m_end_of_data;
  unsigned char *m_byte_position;
  unsigned char *m_start_of_data;
  std::size_t m_mask;

  bool m_out_of_data;

public:
  bit_writer_c(unsigned char *data, std::size_t len)
    : m_end_of_data(data + len)
    , m_byte_position(data)
    , m_start_of_data(data)
    , m_mask(0x80)
    , m_out_of_data(m_byte_position >= m_end_of_data)
  {
  }

  uint64_t copy_bits(std::size_t n, bit_reader_c &src) {
    uint64_t value = src.get_bits(n);
    put_bits(n, value);

    return value;
  }

  inline uint64_t copy_unsigned_golomb(bit_reader_c &r) {
    int n = 0;

    while (r.get_bit() == 0) {
      put_bit(0);
      ++n;
    }

    put_bit(1);

    auto bits = copy_bits(n, r);

    return (1 << n) - 1 + bits;
  }

  inline int64_t copy_signed_golomb(bit_reader_c &r) {
    int64_t v = copy_unsigned_golomb(r);
    return v & 1 ? (v + 1) / 2 : -(v / 2);
  }

  void put_bits(std::size_t n, uint64_t value) {
    while (0 < n) {
      put_bit(value & (1 << (n - 1)));
      --n;
    }
  }

  void put_bit(bool bit) {
    if (m_byte_position >= m_end_of_data) {
      m_out_of_data = true;
      throw mtx::mm_io::end_of_file_x();
    }

    if (bit)
      *m_byte_position |=  m_mask;
    else
      *m_byte_position &= ~m_mask;
    m_mask >>= 1;
    if (0 == m_mask) {
      m_mask = 0x80;
      ++m_byte_position;
      if (m_byte_position == m_end_of_data)
        m_out_of_data = true;
    }
  }

  void byte_align() {
    while (0x80 != m_mask)
      put_bit(0);
  }

  void set_bit_position(std::size_t pos) {
    if (pos >= (static_cast<std::size_t>(m_end_of_data - m_start_of_data) * 8)) {
      m_byte_position = m_end_of_data;
      m_out_of_data   = true;

      throw mtx::mm_io::seek_x();
    }

    m_byte_position = m_start_of_data + (pos / 8);
    m_mask          = 0x80 >> (pos % 8);
  }

  int get_bit_position() {
    std::size_t pos = (m_byte_position - m_start_of_data) * 8;
    for (auto i = 0u; 8 > i; ++i)
      if ((0x80u >> i) == m_mask) {
        pos += i;
        break;
      }
    return pos;
  }

  void skip_bits(unsigned int num) {
    set_bit_position(get_bit_position() + num);
  }

  void skip_bit() {
    set_bit_position(get_bit_position() + 1);
  }
};
using bit_writer_cptr = std::shared_ptr<bit_writer_c>;

#endif // MTX_COMMON_BIT_CURSOR_H
