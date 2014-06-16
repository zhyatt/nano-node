#include <gtest/gtest.h>

#include <mu_coin/mu_coin.hpp>

TEST (wallet, no_key)
{
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    mu_coin::keypair key1;
    mu_coin::uint256_union secret;
    secret.bytes.fill (0);
    mu_coin::private_key prv1;
    ASSERT_TRUE (wallet.fetch (key1.pub, secret, prv1));
}

TEST (wallet, retrieval)
{
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    mu_coin::keypair key1;
    mu_coin::uint256_union secret;
    secret.bytes.fill (0);
    wallet.insert (key1.prv, secret);
    mu_coin::private_key prv1;
    ASSERT_FALSE (wallet.fetch (key1.pub, secret, prv1));
    ASSERT_EQ (key1.prv, prv1);
    secret.bytes [16] ^= 1;
    mu_coin::private_key prv2;
    ASSERT_TRUE (wallet.fetch (key1.pub, secret, prv2));
}

TEST (wallet, empty_iteration)
{
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    auto i (wallet.begin ());
    auto j (wallet.end ());
    ASSERT_EQ (i, j);
}

TEST (wallet, one_item_iteration)
{
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    mu_coin::keypair key1;
    mu_coin::uint256_union secret;
    secret.bytes.fill (0);
    wallet.insert (key1.prv, secret);
    for (auto i (wallet.begin ()), j (wallet.end ()); i != j; ++i)
    {
        mu_coin::public_key key (*i);
        ASSERT_EQ (key1.pub, key);
        mu_coin::private_key prv;
        i.data.key (secret, key1.pub.owords [0], prv);
        ASSERT_EQ (key1.prv, prv);
    }
}

TEST (wallet, two_item_iteration)
{
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    mu_coin::keypair key1;
    mu_coin::keypair key2;
    mu_coin::uint256_union secret;
    secret.bytes.fill (0);
    wallet.insert (key1.prv, secret);
    wallet.insert (key2.prv, secret);
    std::vector <mu_coin::public_key> keys1;
    std::vector <mu_coin::private_key> keys2;
    for (auto i (wallet.begin ()), j (wallet.end ()); i != j; ++i)
    {
        mu_coin::public_key key (*i);
        mu_coin::private_key prv;
        i.data.key (secret, key.owords [0], prv);
        keys1.push_back (key);
        keys2.push_back (prv);
    }
    ASSERT_EQ (2, keys1.size ());
    ASSERT_EQ (2, keys2.size ());
    ASSERT_NE (keys1.end (), std::find (keys1.begin (), keys1.end (), key1.pub));
    ASSERT_NE (keys2.end (), std::find (keys2.begin (), keys2.end (), key1.prv));
    ASSERT_NE (keys1.end (), std::find (keys1.begin (), keys1.end (), key2.pub));
    ASSERT_NE (keys2.end (), std::find (keys2.begin (), keys2.end (), key2.prv));
}

TEST (wallet, insufficient_spend)
{
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    mu_coin::keypair key1;
    mu_coin::uint256_union password;
    auto send (wallet.send (ledger, key1.pub, 500, password));
    ASSERT_EQ (nullptr, send);
}

TEST (wallet, one_spend)
{
    mu_coin::keypair key1;
    mu_coin::uint256_union password;
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    wallet.insert (key1.pub, key1.prv, password);
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    store.genesis_put (key1.pub, 500);
    mu_coin::block_hash latest1;
    store.latest_get (key1.pub, latest1);
    mu_coin::keypair key2;
    auto send (wallet.send (ledger, key2.pub, 500, password));
    ASSERT_NE (nullptr, send);
    ASSERT_EQ (1, send->inputs.size ());
    ASSERT_EQ (1, send->outputs.size ());
    ASSERT_EQ (key1.pub ^ latest1, send->inputs [0].previous);
    ASSERT_EQ (0, send->inputs [0].coins.number ());
    ASSERT_FALSE (mu_coin::validate_message (key1.pub, send->hash (), send->signatures [0]));
    ASSERT_EQ (key2.pub, send->outputs [0].destination);
    ASSERT_EQ (500, send->outputs [0].coins.number ());
}

TEST (wallet, two_spend)
{
    mu_coin::keypair key1;
    mu_coin::keypair key2;
    mu_coin::uint256_union password;
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    wallet.insert (key1.pub, key1.prv, password);
    wallet.insert (key2.pub, key2.prv, password);
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    store.genesis_put (key1.pub, 100);
    mu_coin::block_hash hash1;
    ASSERT_FALSE (store.latest_get (key1.pub, hash1));
    store.genesis_put (key2.pub, 400);
    mu_coin::block_hash hash2;
    ASSERT_FALSE (store.latest_get (key2.pub, hash2));
    mu_coin::keypair key3;
    auto send (wallet.send (ledger, key3.pub, 500, password));
    ASSERT_NE (nullptr, send);
    ASSERT_EQ (2, send->inputs.size ());
    ASSERT_EQ (1, send->outputs.size ());
    ASSERT_EQ (key1.pub ^ hash1, send->inputs [0].previous);
    ASSERT_EQ (0, send->inputs [0].coins.number ());
    ASSERT_FALSE (mu_coin::validate_message (key1.pub, send->hash (), send->signatures [0]));
    ASSERT_EQ (key2.pub ^ hash2, send->inputs [1].previous);
    ASSERT_EQ (0, send->inputs [1].coins.number ());
    ASSERT_FALSE (mu_coin::validate_message (key2.pub, send->hash (), send->signatures [1]));
    ASSERT_EQ (key3.pub, send->outputs [0].destination);
    ASSERT_EQ (500, send->outputs [0].coins.number ());
}

TEST (wallet, partial_spend)
{
    mu_coin::keypair key1;
    mu_coin::uint256_union password;
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    wallet.insert (key1.pub, key1.prv, password);
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    store.genesis_put (key1.pub, 800);
    mu_coin::block_hash latest1;
    ASSERT_FALSE (store.latest_get (key1.pub, latest1));
    mu_coin::keypair key2;
    auto send (wallet.send (ledger, key2.pub, 500, password));
    ASSERT_NE (nullptr, send);
    ASSERT_EQ (1, send->inputs.size ());
    ASSERT_EQ (1, send->outputs.size ());
    ASSERT_EQ (key1.pub ^ latest1, send->inputs [0].previous);
    ASSERT_EQ (300, send->inputs [0].coins.number ());
    ASSERT_FALSE (mu_coin::validate_message (key1.pub, send->hash (), send->signatures [0]));
    ASSERT_EQ (key2.pub, send->outputs [0].destination);
    ASSERT_EQ (500, send->outputs [0].coins.number ());
}

TEST (wallet, spend_no_previous)
{
    mu_coin::keypair key1;
    mu_coin::uint256_union password;
    mu_coin::wallet wallet (mu_coin::wallet_temp);
    for (auto i (0); i < 50; ++i)
    {
        mu_coin::keypair key;
        wallet.insert (key.pub, key.prv, password);
    }
    wallet.insert (key1.pub, key1.prv, password);
    mu_coin::block_store store (mu_coin::block_store_temp);
    mu_coin::ledger ledger (store);
    store.genesis_put (key1.pub, 500);
    mu_coin::block_hash hash1;
    ASSERT_FALSE (store.latest_get (key1.pub, hash1));
    for (auto i (0); i < 50; ++i)
    {
        mu_coin::keypair key;
        wallet.insert (key.pub, key.prv, password);
    }
    mu_coin::keypair key2;
    auto send (wallet.send (ledger, key2.pub, 500, password));
    ASSERT_NE (nullptr, send);
    ASSERT_EQ (1, send->inputs.size ());
    ASSERT_EQ (1, send->outputs.size ());
    ASSERT_EQ (key1.pub ^ hash1, send->inputs [0].previous);
    ASSERT_EQ (0, send->inputs [0].coins.number ());
    ASSERT_FALSE (mu_coin::validate_message (key1.pub, send->hash (), send->signatures [0]));
    ASSERT_EQ (key2.pub, send->outputs [0].destination);
    ASSERT_EQ (500, send->outputs [0].coins.number ());
}