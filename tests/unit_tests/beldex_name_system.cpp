#include "gtest/gtest.h"

#include "common/beldex.h"
#include "cryptonote_core/beldex_name_system.h"
#include "beldex_economy.h"

TEST(beldex_name_system, name_tests)
{
  struct name_test
  {
    std::string name;
    bool allowed;
  };

  name_test const bns_names[] = {
      {"a.bdx", true},
      {"domain.bdx", true},
      {"xn--tda.bdx", true}, // ü
      {"xn--Mnchen-Ost-9db.bdx", true}, // München-Ost
      {"xn--fwg93vdaef749it128eiajklmnopqrstu7dwaxyz0a1a2a3a643qhok169a.bdx", true}, // ⸘🌻‽💩🤣♠♡♢♣🂡🂢🂣🂤🂥🂦🂧🂨🂩🂪🂫🂬🂭🂮🂱🂲🂳🂴🂵🂶🂷🂸🂹
      {"abcdefghijklmnopqrstuvwxyz123456.bdx", true}, // Max length = 32 if no hyphen (so that it can't look like a raw address)
      {"a-cdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456789a.bdx", true}, // Max length = 63 if there is at least one hyphen

      {"abc.domain.bdx", false},
      {"a", false},
      {"a.loko", false},
      {"a domain name.bdx", false},
      {"-.bdx", false},
      {"a_b.bdx", false},
      {" a.bdx", false},
      {"a.bdx ", false},
      {" a.bdx ", false},
      {"localhost.bdx", false},
      {"localhost", false},
      {"beldex.bdx", false},
      {"mnode.bdx", false},
      {"abcdefghijklmnopqrstuvwxyz1234567.bdx", false}, // Too long (no hyphen)
      {"a-cdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456789ab.bdx", false}, // Too long with hyphen
      {"xn--fwg93vdaef749it128eiajklmnopqrstu7dwaxyz0a1a2a3a643qhok169ab.bdx", false}, // invalid (punycode and DNS name parts max at 63)
      {"ab--xyz.bdx", false}, // Double-hyphen at chars 3&4 is reserved by DNS (currently only xn-- is used).
  };

  for (uint16_t type16 = 0; type16 < static_cast<uint16_t>(bns::mapping_type::_count); type16++)
  {
    auto type = static_cast<bns::mapping_type>(type16);
    if (type == bns::mapping_type::wallet) continue; // Not yet supported
    name_test const *names = bns_names;
    size_t names_count     = beldex::char_count(bns_names);

    for (size_t i = 0; i < names_count; i++)
    {
      name_test const &entry = names[i];
      ASSERT_EQ(bns::validate_bns_name(entry.name), entry.allowed) << "Values were {type=" << type << ", name=\"" << entry.name << "\"}";
    }
  }
}

TEST(beldex_name_system, value_encrypt_and_decrypt)
{
  std::string name         = "my bns name";
  bns::mapping_value value = {};
  value.len                = 32;
  memset(&value.buffer[0], 'a', value.len);

  // The type here is not hugely important for decryption except that belnet (as opposed to
  // bchat) doesn't fall back to argon2 decryption if decryption fails.
  constexpr auto type = bns::mapping_type::belnet;

  // Encryption and Decryption success
  {
    auto mval = value;
    ASSERT_TRUE(mval.encrypt(name));
    ASSERT_FALSE(mval == value);
    ASSERT_TRUE(mval.decrypt(name, type));
    ASSERT_TRUE(mval == value);
  }

  // Decryption Fail: Encrypted value was modified
  {
    auto mval = value;
    ASSERT_FALSE(mval.encrypted);
    ASSERT_TRUE(mval.encrypt(name));
    ASSERT_TRUE(mval.encrypted);

    mval.buffer[0] = 'Z';
    ASSERT_FALSE(mval.decrypt(name, type));
    ASSERT_TRUE(mval.encrypted);
  }

  // Decryption Fail: Name was modified
  {
    std::string name_copy = name;
    auto mval = value;
    ASSERT_TRUE(mval.encrypt(name_copy));

    name_copy[0] = 'Z';
    ASSERT_FALSE(mval.decrypt(name_copy, type));
  }
}

TEST(beldex_name_system, value_encrypt_and_decrypt_heavy)
{
  std::string name         = "abcdefg";
  bns::mapping_value value = {};
  value.len                = 33;
  memset(&value.buffer[0], 'a', value.len);

  // Encryption and Decryption success for the older argon2-based encryption key
  {
    auto mval = value;
    auto mval_new = value;
    ASSERT_TRUE(mval.encrypt(name, nullptr, true));
    ASSERT_TRUE(mval_new.encrypt(name, nullptr, false));
    ASSERT_EQ(mval.len + 24, mval_new.len); // New value appends a 24-byte nonce
    ASSERT_TRUE(mval.decrypt(name, bns::mapping_type::bchat));
    ASSERT_TRUE(mval_new.decrypt(name, bns::mapping_type::bchat));
    ASSERT_TRUE(mval == value);
    ASSERT_TRUE(mval_new == value);
  }
}
