#include "common.h"
#include "tls_syntax.h"
#include <gtest/gtest.h>

using namespace mls;

// A struct to test struct encoding, and its operators
struct ExampleStruct
{
  uint16_t a;
  tls::vector<uint8_t, 2> b;
  std::array<uint32_t, 4> c;
};

bool
operator==(const ExampleStruct& lhs, const ExampleStruct& rhs)
{
  return (lhs.a == rhs.a) && (lhs.b == rhs.b) && (lhs.c == rhs.c);
}

tls::ostream&
operator<<(tls::ostream& out, const ExampleStruct& data)
{
  return out << data.a << data.b << data.c;
}

tls::istream&
operator>>(tls::istream& in, ExampleStruct& data)
{
  return in >> data.a >> data.b >> data.c;
}

struct MustInitialize
{
  uint8_t offset;
  uint8_t val;

  MustInitialize(uint8_t offset_in)
    : offset(offset_in)
    , val(0)
  {}

  MustInitialize(uint8_t offset_in, uint8_t val_in)
    : offset(offset_in)
    , val(val_in)
  {}
};

tls::ostream&
operator<<(tls::ostream& out, const MustInitialize& data)
{
  return out << uint8_t(data.offset ^ data.val);
}

tls::istream&
operator>>(tls::istream& in, MustInitialize& data)
{
  in >> data.val;
  data.val ^= data.offset;
  return in;
}

bool
operator==(const MustInitialize& lhs, const MustInitialize& rhs)
{
  return (lhs.offset == rhs.offset) && (lhs.val == rhs.val);
}

// Known-answer tests
class TLSSyntaxTest : public ::testing::Test
{
protected:
  const uint8_t val_uint8{ 0x11 };
  const bytes enc_uint8 = from_hex("11");

  const uint16_t val_uint16{ 0x2222 };
  const bytes enc_uint16 = from_hex("2222");

  const uint32_t val_uint32{ 0x44444444 };
  const bytes enc_uint32 = from_hex("44444444");

  const uint64_t val_uint64{ 0x8888888888888888 };
  const bytes enc_uint64 = from_hex("8888888888888888");

  const std::array<uint16_t, 4> val_array{ 1, 2, 3, 4 };
  const bytes enc_array = from_hex("0001000200030004");

  const tls::vector<uint32_t, 3> val_vector{ 5, 6 };
  const bytes enc_vector = from_hex("0000080000000500000006");

  const ExampleStruct val_struct{
    0x1111,
    { 0x22, 0x22 },
    { 0x33333333, 0x44444444, 0x55555555, 0x66666666 }
  };
  const bytes enc_struct =
    from_hex("11110002222233333333444444445555555566666666");

  const tls::optional<ExampleStruct> val_optional{ val_struct };
  const bytes enc_optional = from_hex("01") + enc_struct;

  const tls::optional<ExampleStruct> val_optional_null = std::nullopt;
  const bytes enc_optional_null = from_hex("00");

  const uint8_t variant_param = 0xff;
  typedef tls::variant_vector<MustInitialize, uint8_t, 1> test_var_vector;
  typedef tls::variant_optional<MustInitialize, uint8_t> test_var_optional;

  test_var_vector val_var_vector;
  const bytes enc_var_vector = from_hex("02f00f");

  test_var_optional val_var_optional;
  const bytes enc_var_optional = from_hex("01f0");

  TLSSyntaxTest()
    : val_var_vector(variant_param)
    , val_var_optional(variant_param)
  {
    val_var_vector.push_back({ 0xff, 0x0f });
    val_var_vector.push_back({ 0xff, 0xf0 });
    val_var_optional = MustInitialize{ 0xff, 0x0f };
  }
};

template<typename T>
void
ostream_test(T val, const std::vector<uint8_t>& enc)
{
  tls::ostream w;
  w << val;
  ASSERT_EQ(w.bytes(), enc);
}

TEST_F(TLSSyntaxTest, OStream)
{
  bytes answer{ 1, 2, 3, 4 };
  tls::ostream w;
  w.write_raw(answer);
  ASSERT_EQ(w.bytes(), answer);

  ostream_test(val_uint8, enc_uint8);
  ostream_test(val_uint16, enc_uint16);
  ostream_test(val_uint32, enc_uint32);
  ostream_test(val_uint64, enc_uint64);
  ostream_test(val_array, enc_array);
  ostream_test(val_vector, enc_vector);
  ostream_test(val_struct, enc_struct);
  ostream_test(val_optional, enc_optional);
  ostream_test(val_optional_null, enc_optional_null);
  ostream_test(val_var_vector, enc_var_vector);
  ostream_test(val_var_optional, enc_var_optional);
}

template<typename T>
void
istream_test(T val, T& data, const std::vector<uint8_t>& enc)
{
  tls::istream r(enc);
  r >> data;
  ASSERT_EQ(data, val);
}

TEST_F(TLSSyntaxTest, IStream)
{
  uint8_t data_uint8;
  istream_test(val_uint8, data_uint8, enc_uint8);

  uint16_t data_uint16;
  istream_test(val_uint16, data_uint16, enc_uint16);

  uint32_t data_uint32;
  istream_test(val_uint32, data_uint32, enc_uint32);

  uint64_t data_uint64;
  istream_test(val_uint64, data_uint64, enc_uint64);

  std::array<uint16_t, 4> data_array;
  istream_test(val_array, data_array, enc_array);

  tls::vector<uint32_t, 3> data_vector;
  istream_test(val_vector, data_vector, enc_vector);

  ExampleStruct data_struct;
  istream_test(val_struct, data_struct, enc_struct);

  tls::optional<ExampleStruct> data_optional;
  istream_test(val_optional, data_optional, enc_optional);

  tls::optional<ExampleStruct> data_optional_null;
  istream_test(val_optional_null, data_optional_null, enc_optional_null);

  test_var_vector data_var_vector(variant_param);
  istream_test(val_var_vector, data_var_vector, enc_var_vector);

  test_var_optional data_var_optional(variant_param);
  istream_test(val_var_optional, data_var_optional, enc_var_optional);
}

// TODO(rlb@ipv.sx) Test failure cases
