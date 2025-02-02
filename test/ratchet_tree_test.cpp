#include "messages.h"
#include "ratchet_tree.h"
#include "test_vectors.h"
#include "tls_syntax.h"
#include <gtest/gtest.h>

using namespace mls;

class RatchetTreeTest : public ::testing::Test
{
protected:
  const CipherSuite suite = CipherSuite::P256_SHA256_AES128GCM;
  const SignatureScheme scheme = SignatureScheme::Ed25519;

  Credential credA;
  Credential credB;
  Credential credC;
  Credential credD;

  const bytes secretA = from_hex("00010203");
  const bytes secretB = from_hex("04050607");
  const bytes secretC = from_hex("08090a0b");
  const bytes secretD = from_hex("0c0d0e0f");

  const bytes secretAB = from_hex(
    "e8de418a07b497953174c71f5ad83d63d90bc68582a9a340c6023fba536455f4");
  const bytes secretABC = from_hex(
    "1dbd153c8f2ca387cfc3104b39b0954bbf287bfeb94d2a5bd92e05ff510c2244");
  const bytes secretABCD = from_hex(
    "ca118da171367f30e5c03e2e651558f55c57fba6319101ccb56f8a34953b25f2");

  // Manually computed via a Python script
  const bytes hashA = from_hex(
    "30a1ceecab0b150dd15d1a851d7ed36923e872d7344aea6197a8a82f943266f6");
  const bytes hashB = from_hex(
    "c314eb7da4e11019cd105c1935f6e94000cc0059c1b35b2fd661f1a2a722c857");
  const bytes hashC = from_hex(
    "b5b1bf5e264c2c3ec60faf5abcd9f69d076674774ff929337ccb345ea4fd983a");
  const bytes hashD = from_hex(
    "ed1526270fdca2222730bb48c92825c7518399dfb266ecd1c8564e2b36f63d71");
  const bytes hashAB = from_hex(
    "34c656e7bf8b535f5781e561f3b5115946e373c108ace3cf9bbae09e6963530e");
  const bytes hashCD = from_hex(
    "775c50add1cf6ab09c84d67c050a97d5290d541453292fe35e033ca3e416bfd5");
  const bytes hashABC = from_hex(
    "0107f66c071a4c073af7f42de07f014e386bf9d47bfcabe366985e9d228b8c79");
  const bytes hashABCD = from_hex(
    "0121303fd31ce16c64328e4fea1cdb3232387f7929f4754463484a927363d572");

  const TreeTestVectors& tv;

  RatchetTreeTest()
    : tv(TestLoader<TreeTestVectors>::get())
  {
    auto sigA = SignaturePrivateKey::derive(scheme, secretA);
    credA = Credential::basic({ 'A' }, sigA.public_key());

    auto sigB = SignaturePrivateKey::derive(scheme, secretB);
    credB = Credential::basic({ 'B' }, sigB.public_key());

    auto sigC = SignaturePrivateKey::derive(scheme, secretC);
    credC = Credential::basic({ 'C' }, sigC.public_key());

    auto sigD = SignaturePrivateKey::derive(scheme, secretD);
    credD = Credential::basic({ 'D' }, sigD.public_key());
  }

  void assert_tree_eq(const TreeTestVectors::TreeCase& vec,
                      const TestRatchetTree& tree)
  {
    auto& nodes = tree.nodes();
    ASSERT_EQ(vec.size(), nodes.size());

    for (size_t j = 0; j < vec.size(); ++j) {
      ASSERT_EQ(vec[j].hash, nodes[j].hash());
      ASSERT_EQ(vec[j].public_key.has_value(), nodes[j].has_value());
      if (nodes[j].has_value()) {
        ASSERT_EQ(vec[j].public_key.value(), nodes[j]->public_key().to_bytes());
      }
    }
  }

  void interop(const TreeTestVectors::TestCase& tc,
               CipherSuite test_suite,
               SignatureScheme test_scheme)
  {
    TestRatchetTree tree{ test_suite };

    // Add the leaves
    int tci = 0;
    for (uint32_t i = 0; i < tv.leaf_secrets.size(); ++i, ++tci) {
      tree.add_leaf(LeafIndex{ i }, tv.leaf_secrets[i], tc.credentials[i]);
      tree.set_path(LeafIndex{ i }, tv.leaf_secrets[i]);
      assert_tree_eq(tc.trees[tci], tree);
    }

    // Blank even-numbered leaves
    for (uint32_t j = 0; j < tv.leaf_secrets.size(); j += 2, ++tci) {
      tree.blank_path(LeafIndex{ j });
      assert_tree_eq(tc.trees[tci], tree);
    }
  }
};

TEST_F(RatchetTreeTest, Interop)
{
  interop(tv.case_p256_p256,
          CipherSuite::P256_SHA256_AES128GCM,
          SignatureScheme::P256_SHA256);
  interop(tv.case_x25519_ed25519,
          CipherSuite::X25519_SHA256_AES128GCM,
          SignatureScheme::Ed25519);
}

TEST_F(RatchetTreeTest, OneMember)
{
  RatchetTree tree{ suite, secretA, credA };
  ASSERT_EQ(tree.size(), 1);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 0 }), credA);
}

TEST_F(RatchetTreeTest, MultipleMembers)
{
  RatchetTree tree{ suite,
                    { secretA, secretB, secretC, secretD },
                    { credA, credB, credC, credD } };
  ASSERT_EQ(tree.size(), 4);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 0 }), credA);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 1 }), credB);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 2 }), credC);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 3 }), credD);
}

TEST_F(RatchetTreeTest, ByExtension)
{
  RatchetTree tree{ suite };
  bytes root_secret;

  // Add A
  tree.add_leaf(LeafIndex{ 0 }, secretA, credA);
  root_secret = tree.set_path(LeafIndex{ 0 }, secretA);
  ASSERT_EQ(tree.root_hash(), hashA);
  ASSERT_EQ(root_secret, secretA);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 0 }), credA);

  // Add B
  tree.add_leaf(LeafIndex{ 1 }, secretB, credB);
  root_secret = tree.set_path(LeafIndex{ 1 }, secretB);

  ASSERT_EQ(tree.size(), 2);
  ASSERT_EQ(root_secret, secretAB);
  ASSERT_EQ(tree.root_hash(), hashAB);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 0 }), credA);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 1 }), credB);

  RatchetTree directAB{ suite, { secretA, secretB }, { credA, credB } };
  ASSERT_EQ(tree, directAB);

  // Add C
  tree.add_leaf(LeafIndex{ 2 }, secretC, credC);
  root_secret = tree.set_path(LeafIndex{ 2 }, secretC);

  ASSERT_EQ(tree.size(), 3);
  ASSERT_EQ(root_secret, secretABC);
  ASSERT_EQ(tree.root_hash(), hashABC);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 0 }), credA);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 1 }), credB);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 2 }), credC);

  RatchetTree directABC{ suite,
                         { secretA, secretB, secretC },
                         { credA, credB, credC } };
  ASSERT_EQ(tree, directABC);

  // Add D
  tree.add_leaf(LeafIndex{ 3 }, secretD, credD);
  root_secret = tree.set_path(LeafIndex{ 3 }, secretD);

  ASSERT_EQ(tree.size(), 4);
  ASSERT_EQ(root_secret, secretABCD);
  ASSERT_EQ(tree.root_hash(), hashABCD);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 0 }), credA);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 1 }), credB);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 2 }), credC);
  ASSERT_EQ(tree.get_credential(LeafIndex{ 3 }), credD);

  RatchetTree direct{ suite,
                      { secretA, secretB, secretC, secretD },
                      { credA, credB, credC, credD } };
  ASSERT_EQ(tree, direct);
}

TEST_F(RatchetTreeTest, BySerialization)
{
  RatchetTree before{ suite,
                      { secretA, secretB, secretC, secretD },
                      { credA, credB, credC, credD } };
  RatchetTree after{ suite };

  tls::unmarshal(tls::marshal(before), after);
  ASSERT_EQ(before, after);
}

TEST_F(RatchetTreeTest, BySerializationWithBlanks)
{
  RatchetTree before{ suite,
                      { secretA, secretB, secretC, secretD },
                      { credA, credB, credC, credD } };
  RatchetTree after{ suite };

  before.blank_path(LeafIndex{ 1 });
  tls::unmarshal(tls::marshal(before), after);
  ASSERT_EQ(before, after);
}

TEST_F(RatchetTreeTest, EncryptDecrypt)
{
  size_t size = 5;

  // trees[i] represents a tree with a private key for only leaf i
  std::vector<RatchetTree> trees(size, { suite });
  for (LeafIndex i{ 0 }; i.val < size; i.val += 1) {
    auto secret = random_bytes(32);
    auto priv = DHPrivateKey::node_derive(suite, secret);
    auto pub = priv.public_key();
    auto sig = SignaturePrivateKey::derive(scheme, secret);
    auto cred = Credential::basic({ uint8_t(i.val) }, sig.public_key());

    for (uint32_t j = 0; j < size; j += 1) {
      if (i.val == j) {
        trees[j].add_leaf(i, secret, cred);
      } else {
        trees[j].add_leaf(i, pub, cred);
      }
    }
  }

  for (uint32_t i = 0; i < size; ++i) {
    EXPECT_EQ(trees[i], trees[0]);
    ASSERT_EQ(trees[i].size(), size);
    ASSERT_TRUE(trees[i].check_credentials());
    ASSERT_TRUE(trees[i].check_invariant(LeafIndex{ i }));
  }

  // Verify that each member can encrypt and be decrypted by the
  // other members
  for (LeafIndex i{ 0 }; i.val < size; i.val += 1) {
    auto secret = random_bytes(32);

    DirectPath path(trees[i.val].cipher_suite());
    bytes root_path_secret;
    std::tie(path, root_path_secret) = trees[i.val].encrypt(i, secret);

    for (size_t j = 0; j < size; ++j) {
      if (i.val == j) {
        auto update_secret = trees[j].set_path(i, secret);
        ASSERT_EQ(update_secret, root_path_secret);
      } else {
        auto info = trees[j].decrypt(i, path);
        ASSERT_EQ(info.root_path_secret, root_path_secret);
        trees[j].merge_path(i, info);
      }
    }

    for (uint32_t j = 0; j < size; ++j) {
      ASSERT_EQ(trees[i.val], trees[j]);
      ASSERT_TRUE(trees[j].check_invariant(LeafIndex{ j }));
    }
  }
}
