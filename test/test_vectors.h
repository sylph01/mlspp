#include "common.h"
#include "messages.h"
#include "session.h"
#include "state.h"
#include "tls_syntax.h"
#include "tree_math.h"
#include <string>

using namespace mls;

/////

struct TreeMathTestVectors
{
  static const std::string file_name;

  LeafCount n_leaves{ 255 };
  tls::vector<NodeIndex, 4> root;
  tls::vector<NodeIndex, 4> left;
  tls::vector<NodeIndex, 4> right;
  tls::vector<NodeIndex, 4> parent;
  tls::vector<NodeIndex, 4> sibling;
};

tls::istream&
operator>>(tls::istream& str, TreeMathTestVectors& tv);
tls::ostream&
operator<<(tls::ostream& str, const TreeMathTestVectors& tv);

/////

struct ResolutionTestVectors
{
  static const std::string file_name;

  typedef tls::vector<uint8_t, 1> Resolution;
  typedef tls::vector<Resolution, 2> ResolutionCase;

  static std::vector<bool> make_tree(uint32_t t, NodeCount w);
  static std::vector<uint8_t> compact(const std::vector<NodeIndex>& res);

  LeafCount n_leaves;
  tls::vector<ResolutionCase, 4> cases;
};

tls::istream&
operator>>(tls::istream& str, ResolutionTestVectors& tv);
tls::ostream&
operator<<(tls::ostream& str, const ResolutionTestVectors& tv);

/////

struct CryptoTestVectors
{
  static const std::string file_name;

  tls::opaque<1> hkdf_extract_salt;
  tls::opaque<1> hkdf_extract_ikm;

  tls::opaque<1> derive_secret_secret;
  tls::opaque<1> derive_secret_label;
  tls::opaque<1> derive_secret_context;

  tls::opaque<1> derive_key_pair_seed;

  tls::opaque<1> ecies_plaintext;

  struct TestCase
  {
    // HKDF-Extract
    tls::opaque<1> hkdf_extract_out;

    // Derive-Secret
    tls::opaque<1> derive_secret_out;

    // Derive-Key-Pair
    DHPublicKey derive_key_pair_pub;

    // HPKE
    HPKECiphertext ecies_out;

    TestCase(CipherSuite suite)
      : derive_key_pair_pub(suite)
      , ecies_out(suite)
    {}
  };

  CryptoTestVectors()
    : case_p256(CipherSuite::P256_SHA256_AES128GCM)
    , case_x25519(CipherSuite::X25519_SHA256_AES128GCM)
  {}

  TestCase case_p256;
  TestCase case_x25519;
};

tls::istream&
operator>>(tls::istream& str, CryptoTestVectors& tv);
tls::ostream&
operator<<(tls::ostream& str, const CryptoTestVectors& tv);

/////

struct KeyScheduleTestVectors
{
  static const std::string file_name;

  struct Epoch
  {
    tls::opaque<1> update_secret;

    tls::opaque<1> epoch_secret;
    tls::opaque<1> application_secret;
    tls::opaque<1> confirmation_key;
    tls::opaque<1> init_secret;
  };

  struct TestCase
  {
    CipherSuite suite;
    tls::vector<Epoch, 2> epochs;
  };

  uint32_t n_epochs;
  tls::opaque<4> base_group_context;

  TestCase case_p256;
  TestCase case_x25519;
};

tls::istream&
operator>>(tls::istream& str, KeyScheduleTestVectors& tv);
tls::ostream&
operator<<(tls::ostream& str, const KeyScheduleTestVectors& tv);

/////

struct AppKeyScheduleTestVectors
{
  static const std::string file_name;

  struct Step
  {
    tls::opaque<1> secret;
    tls::opaque<1> key;
    tls::opaque<1> nonce;
  };

  typedef tls::vector<Step, 4> KeySequence;
  typedef tls::vector<KeySequence, 4> TestCase;

  uint32_t n_members;
  uint32_t n_generations;
  tls::opaque<1> application_secret;

  TestCase case_p256;
  TestCase case_x25519;
};

tls::istream&
operator>>(tls::istream& str, AppKeyScheduleTestVectors& tv);
tls::ostream&
operator<<(tls::ostream& str, const AppKeyScheduleTestVectors& tv);

/////

class TestRatchetTree : public RatchetTree
{
public:
  using RatchetTree::RatchetTree;
  const RatchetTreeNodeVector& nodes() const { return _nodes; }
};

struct TreeTestVectors
{
  static const std::string file_name;

  struct TreeNode
  {
    tls::optional<tls::opaque<1>> public_key;
    tls::opaque<1> hash;
  };

  typedef tls::vector<TreeNode, 4> TreeCase;

  struct TestCase
  {
    tls::vector<Credential, 4> credentials;
    tls::vector<TreeCase, 4> trees;
  };

  tls::vector<tls::opaque<1>, 4> leaf_secrets;
  tls::vector<Credential, 4> credentials;
  TestCase case_p256_p256;
  TestCase case_x25519_ed25519;
};

tls::istream&
operator>>(tls::istream& str, TreeTestVectors& tv);
tls::ostream&
operator<<(tls::ostream& str, const TreeTestVectors& tv);

/////

struct MessagesTestVectors
{
  static const std::string file_name;

  struct TestCase
  {
    CipherSuite cipher_suite;
    SignatureScheme sig_scheme;

    tls::opaque<4> client_init_key;
    tls::opaque<4> welcome_info;
    tls::opaque<4> welcome;
    tls::opaque<4> add;
    tls::opaque<4> update;
    tls::opaque<4> remove;
    tls::opaque<4> ciphertext;
  };

  epoch_t epoch;
  LeafIndex signer_index;
  LeafIndex removed;
  tls::opaque<1> user_id;
  tls::opaque<1> group_id;
  tls::opaque<1> client_init_key_id;
  tls::opaque<1> dh_seed;
  tls::opaque<1> sig_seed;
  tls::opaque<1> random;

  SignatureScheme cik_all_scheme;
  tls::opaque<4> client_init_key_all;

  TestCase case_p256_p256;
  TestCase case_x25519_ed25519;
};

tls::istream&
operator>>(tls::istream& str, MessagesTestVectors::TestCase& tv);
tls::ostream&
operator<<(tls::ostream& str, const MessagesTestVectors::TestCase& tc);

tls::istream&
operator>>(tls::istream& str, MessagesTestVectors& tv);
tls::ostream&
operator<<(tls::ostream& str, const MessagesTestVectors& tv);

/////

namespace mls {

class TestSession : public Session
{
public:
  using Session::Session;
  TestSession(const Session& other)
    : Session(other)
  {}

  uint32_t index() const { return current_state().index().val; }

  epoch_t current_epoch() const { return _current_epoch; }

  CipherSuite cipher_suite() const { return current_state().cipher_suite(); }

  bytes current_epoch_secret() const { return current_state().epoch_secret(); }

  bytes current_application_secret() const
  {
    return current_state().application_secret();
  }

  bytes current_confirmation_key() const
  {
    return current_state().confirmation_key();
  }

  bytes current_init_secret() const { return current_state().init_secret(); }
};

} // namespace mls

// Splitting the test data from the file definition here allows us
// to have a consistent struct for different scenarios that live in
// different files.
struct SessionTestVectors
{
  struct Epoch
  {
    tls::optional<Welcome> welcome;
    tls::opaque<4> handshake;

    epoch_t epoch;
    tls::opaque<1> epoch_secret;
    tls::opaque<1> application_secret;
    tls::opaque<1> confirmation_key;
    tls::opaque<1> init_secret;

    Epoch() = default;

    Epoch(const tls::optional<Welcome>& welcome_in,
          const bytes& handshake_in,
          const TestSession& session)
      : welcome(welcome_in)
      , handshake(handshake_in)
      , epoch(session.current_epoch())
      , epoch_secret(session.current_epoch_secret())
      , application_secret(session.current_application_secret())
      , confirmation_key(session.current_confirmation_key())
      , init_secret(session.current_init_secret())
    {}
  };

  struct TestCase
  {
    CipherSuite cipher_suite;
    SignatureScheme sig_scheme;
    tls::vector<ClientInitKey, 4> client_init_keys;
    tls::vector<Epoch, 4> transcript;
  };

  uint32_t group_size;
  tls::opaque<1> group_id;

  TestCase case_p256_p256;
  TestCase case_x25519_ed25519;
};

tls::istream&
operator>>(tls::istream& str, SessionTestVectors::TestCase& tv);
tls::ostream&
operator<<(tls::ostream& str, const SessionTestVectors::TestCase& tv);

tls::istream&
operator>>(tls::istream& str, SessionTestVectors& tv);
tls::ostream&
operator<<(tls::ostream& str, const SessionTestVectors& tv);

struct BasicSessionTestVectors : SessionTestVectors
{
  static const std::string file_name;
};

/////

template<typename T>
struct TestLoader
{
  static const T& get();

private:
  static bool _initialized;
  static T _vectors;
};
