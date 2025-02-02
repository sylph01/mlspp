#include "state.h"
#include "test_vectors.h"
#include <gtest/gtest.h>

using namespace mls;

class AppKeyScheduleTest : public ::testing::Test
{
protected:
  const AppKeyScheduleTestVectors& tv;

  AppKeyScheduleTest()
    : tv(TestLoader<AppKeyScheduleTestVectors>::get())
  {}

  void interop(CipherSuite suite, const AppKeyScheduleTestVectors::TestCase& tc)
  {
    ASSERT_EQ(tc.size(), tv.n_members);
    KeyChain chain(suite);
    chain.start(LeafIndex{ 0 }, tv.application_secret);
    for (uint32_t j = 0; j < tv.n_members; ++j) {
      ASSERT_EQ(tc[j].size(), tv.n_generations);
      for (uint32_t k = 0; k < tv.n_generations; ++k) {
        auto kn = chain.get(LeafIndex{ j }, k);
        ASSERT_EQ(tc[j][k].secret, kn.secret);
        ASSERT_EQ(tc[j][k].key, kn.key);
        ASSERT_EQ(tc[j][k].nonce, kn.nonce);
      }
    }
  }
};

TEST_F(AppKeyScheduleTest, Interop)
{
  interop(CipherSuite::P256_SHA256_AES128GCM, tv.case_p256);
  interop(CipherSuite::X25519_SHA256_AES128GCM, tv.case_x25519);
}

class StateTest : public ::testing::Test
{
protected:
  const CipherSuite suite = CipherSuite::P256_SHA256_AES128GCM;
  const SignatureScheme scheme = SignatureScheme::P256_SHA256;

  const size_t group_size = 5;
  const bytes group_id = { 0, 1, 2, 3 };
  const bytes user_id = { 4, 5, 6, 7 };
};

class GroupCreationTest : public StateTest
{
protected:
  std::vector<SignaturePrivateKey> identity_privs;
  std::vector<Credential> credentials;
  std::vector<DHPrivateKey> init_privs;
  std::vector<ClientInitKey> user_init_keys;
  std::vector<State> states;

  const bytes test_message = from_hex("01020304");

  GroupCreationTest()
  {
    for (size_t i = 0; i < group_size; i += 1) {
      auto identity_priv = SignaturePrivateKey::generate(scheme);
      auto credential = Credential::basic(user_id, identity_priv);
      auto init_priv = DHPrivateKey::generate(suite);

      auto user_init_key = ClientInitKey{};
      user_init_key.add_init_key(init_priv);
      user_init_key.sign(credential);

      identity_privs.push_back(identity_priv);
      credentials.push_back(credential);
      init_privs.push_back(init_priv);
      user_init_keys.push_back(user_init_key);
    }
  }
};

TEST_F(GroupCreationTest, TwoPerson)
{
  // Initialize the creator's state
  auto first = State{ group_id, suite, init_privs[0], credentials[0] };

  // Create a Add for the new participant
  auto welcome_add_state = first.add(user_init_keys[1]);
  auto welcome = std::get<0>(welcome_add_state);
  auto add = std::get<1>(welcome_add_state);

  // Process the Add
  first = std::get<2>(welcome_add_state);
  auto second = State{ user_init_keys[1], welcome, add };

  ASSERT_EQ(first, second);

  // Verify that they can exchange protected messages
  auto encrypted = first.protect(test_message);
  auto decrypted = second.unprotect(encrypted);
  ASSERT_EQ(decrypted, test_message);
}

TEST_F(GroupCreationTest, FullSize)
{
  // Initialize the creator's state
  states.emplace_back(group_id, suite, init_privs[0], credentials[0]);

  // Each participant invites the next
  for (size_t i = 1; i < group_size; i += 1) {
    auto sender = i - 1;
    auto welcome_add_state = states[sender].add(user_init_keys[i]);
    auto welcome = std::get<0>(welcome_add_state);
    auto add = std::get<1>(welcome_add_state);

    for (size_t j = 0; j < states.size(); j += 1) {
      if (j == sender) {
        states[j] = std::get<2>(welcome_add_state);
      } else {
        states[j] = states[j].handle(add);
      }
    }

    states.emplace_back(user_init_keys[i], welcome, add);

    // Check that everyone ended up in the same place
    for (const auto& state : states) {
      ASSERT_EQ(state, states[0]);
    }

    // Check that everyone can send and be received
    for (auto& state : states) {
      auto encrypted = state.protect(test_message);
      for (auto& other : states) {
        auto decrypted = other.unprotect(encrypted);
        ASSERT_EQ(decrypted, test_message);
      }
    }
  }
}

class RunningGroupTest : public StateTest
{
protected:
  std::vector<State> states;

  RunningGroupTest()
  {
    states.reserve(group_size);

    auto init_secret_0 = random_bytes(32);
    auto init_priv_0 = DHPrivateKey::derive(suite, init_secret_0);
    auto identity_priv_0 = SignaturePrivateKey::generate(scheme);
    auto credential_0 = Credential::basic(user_id, identity_priv_0);
    states.emplace_back(group_id, suite, init_priv_0, credential_0);

    for (size_t i = 1; i < group_size; i += 1) {
      auto init_secret = random_bytes(32);
      auto init_priv = DHPrivateKey::node_derive(suite, init_secret);
      auto identity_priv = SignaturePrivateKey::generate(scheme);
      auto credential = Credential::basic(user_id, identity_priv);

      ClientInitKey cik;
      cik.add_init_key(init_priv);
      cik.sign(credential);

      auto welcome_add_state = states[0].add(cik);
      auto&& welcome = std::get<0>(welcome_add_state);
      auto&& add = std::get<1>(welcome_add_state);
      auto&& next = std::get<2>(welcome_add_state);
      for (auto& state : states) {
        if (state.index().val == 0) {
          state = next;
        } else {
          state = state.handle(add);
        }
      }

      states.emplace_back(cik, welcome, add);
    }
  }

  void SetUp() override { check_consistency(); }

  void check_consistency()
  {
    for (const auto& state : states) {
      ASSERT_EQ(state, states[0]);
    }
  }
};

TEST_F(RunningGroupTest, Update)
{
  for (size_t i = 0; i < group_size; i += 1) {
    auto new_leaf = random_bytes(32);
    auto message_next = states[i].update(new_leaf);
    auto&& message = std::get<0>(message_next);
    auto&& next = std::get<1>(message_next);

    for (auto& state : states) {
      if (state.index().val == i) {
        state = next;
      } else {
        state = state.handle(message);
      }
    }

    check_consistency();
  }
}

TEST_F(RunningGroupTest, Remove)
{
  for (int i = group_size - 2; i > 0; i -= 1) {
    auto evict_secret = random_bytes(32);
    auto message_next =
      states[i].remove(evict_secret, LeafIndex{ uint32_t(i + 1) });
    auto&& message = std::get<0>(message_next);
    auto&& next = std::get<1>(message_next);
    states.pop_back();

    for (auto& state : states) {
      if (state.index().val == size_t(i)) {
        state = next;
      } else {
        state = state.handle(message);
      }
    }

    check_consistency();
  }
}

TEST(OtherStateTest, CipherNegotiation)
{
  // Alice supports P-256 and X25519
  auto idkA = SignaturePrivateKey::generate(SignatureScheme::Ed25519);
  auto credA = Credential::basic({ 0, 1, 2, 3 }, idkA);
  auto insA = bytes{ 0, 1, 2, 3 };
  auto inkA1 =
    DHPrivateKey::node_derive(CipherSuite::P256_SHA256_AES128GCM, insA);
  auto inkA2 =
    DHPrivateKey::node_derive(CipherSuite::X25519_SHA256_AES128GCM, insA);

  auto cikA = ClientInitKey{};
  cikA.add_init_key(inkA1);
  cikA.add_init_key(inkA2);
  cikA.sign(credA);

  // Bob spuports P-256 and P-521
  auto supported_ciphers =
    std::vector<CipherSuite>{ CipherSuite::P256_SHA256_AES128GCM,
                              CipherSuite::P521_SHA512_AES256GCM };
  auto idkB = SignaturePrivateKey::generate(SignatureScheme::Ed25519);
  auto credB = Credential::basic({ 4, 5, 6, 7 }, idkB);
  auto insB = bytes{ 4, 5, 6, 7 };
  auto inkB =
    DHPrivateKey::node_derive(CipherSuite::P256_SHA256_AES128GCM, insB);
  auto group_id = bytes{ 0, 1, 2, 3, 4, 5, 6, 7 };

  auto cikB = ClientInitKey{};
  cikB.add_init_key(inkB);
  cikB.sign(credB);

  // Bob should choose P-256
  auto initialB = State::negotiate(group_id, cikB, cikA);
  auto stateB = std::get<2>(initialB);
  ASSERT_EQ(stateB.cipher_suite(), CipherSuite::P256_SHA256_AES128GCM);

  // Alice should also arrive at P-256 when initialized
  auto welcome = std::get<0>(initialB);
  auto add = std::get<1>(initialB);
  auto stateA = State(cikA, welcome, add);
  ASSERT_EQ(stateA, stateB);
}

class KeyScheduleTest : public ::testing::Test
{
protected:
  const KeyScheduleTestVectors& tv;

  KeyScheduleTest()
    : tv(TestLoader<KeyScheduleTestVectors>::get())
  {}

  void interop(const KeyScheduleTestVectors::TestCase& test_case)
  {
    auto suite = test_case.suite;
    auto secret_size = Digest(suite).output_size();
    bytes init_secret(secret_size, 0);

    GroupContext group_context;
    tls::unmarshal(tv.base_group_context, group_context);

    for (const auto& epoch : test_case.epochs) {
      auto group_context_bytes = tls::marshal(group_context);
      auto secrets = State::derive_epoch_secrets(
        suite, init_secret, epoch.update_secret, group_context_bytes);
      ASSERT_EQ(epoch.epoch_secret, secrets.epoch_secret);
      ASSERT_EQ(epoch.application_secret, secrets.application_secret);
      ASSERT_EQ(epoch.confirmation_key, secrets.confirmation_key);
      ASSERT_EQ(epoch.init_secret, secrets.init_secret);

      group_context.epoch += 1;
      init_secret = secrets.init_secret;
    }
  }
};

TEST_F(KeyScheduleTest, Interop)
{
  interop(tv.case_p256);
  interop(tv.case_x25519);
}
