/*******************************************************************************
*   (c) 2018 Zondax GmbH
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "gtest/gtest.h"
#include <json/json_parser.h>
#include <tx_display.h>
#include <tx_parser.h>
#include <common/parser.h>
#include "common.h"

namespace {
#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantParameter"
    parser_error_t tx_traverse(int16_t root_token_index, uint8_t *numChunks) {
        uint16_t ret_value_token_index = 0;
        const parser_error_t err = tx_traverse_find(root_token_index, &ret_value_token_index);

        if (err != parser_ok){
            return err;
        }

        return tx_getToken(ret_value_token_index,
                           parser_tx_obj.tx_json.query.out_val, parser_tx_obj.tx_json.query.out_val_len,
                           parser_tx_obj.tx_json.query.page_index, numChunks);
    }
#pragma clang diagnostic pop

    TEST(TxParse, Tx_Traverse) {
        auto transaction = R"({"keyA":"123456", "keyB":"abcdefg", "keyC":""})";

        parser_tx_obj.tx_json.tx = transaction;
        parser_tx_obj.tx_json.flags.cache_valid = false;
        parser_error_t err = JSON_PARSE(&parser_tx_obj.tx_json.json, parser_tx_obj.tx_json.tx);

        ASSERT_EQ(err, parser_ok);
        // Check some tokens
        ASSERT_EQ(parser_tx_obj.tx_json.json.numberOfTokens, 7) << "It should contain 7 = 1 (dict) + 6 (key+value)";
        ASSERT_EQ(parser_tx_obj.tx_json.json.tokens[0].start, 0);
        ASSERT_EQ(parser_tx_obj.tx_json.json.tokens[0].end, 46);
        ASSERT_EQ(parser_tx_obj.tx_json.json.tokens[0].size, 3) << "size should be 3 = 3 key/values contained in the dict";
        ASSERT_EQ(parser_tx_obj.tx_json.json.tokens[3].start, 19);
        ASSERT_EQ(parser_tx_obj.tx_json.json.tokens[3].end, 23);

        char key[100];
        char val[100];
        uint8_t numChunks;

        // Try second key - first chunk
        INIT_QUERY_CONTEXT(key, sizeof(key), val, sizeof(val), 0, 4)
        parser_tx_obj.tx_json.query.item_index = 1;

        err = tx_traverse(0, &numChunks);
        EXPECT_EQ(err, parser_ok) << parser_getErrorDescription(err);
        EXPECT_EQ(numChunks, 1) << "Incorrect number of chunks";
        EXPECT_EQ_STR(key, "keyB", "Incorrect key")
        EXPECT_EQ_STR(val, "abcdefg", "Incorrect value")

        // Try second key - Second chunk
        INIT_QUERY_CONTEXT(key, sizeof(key), val, sizeof(val), 1, 4)
        parser_tx_obj.tx_json.query.item_index = 1;
        err = tx_traverse(0, &numChunks);
        EXPECT_EQ(err, parser_display_page_out_of_range) << parser_getErrorDescription(err);
        EXPECT_EQ(numChunks, 1) << "Incorrect number of chunks";

        // Find first key
        INIT_QUERY_CONTEXT(key, sizeof(key), val, sizeof(val), 0, 4)
        parser_tx_obj.tx_json.query.item_index = 0;
        err = tx_traverse(0, &numChunks);
        EXPECT_EQ(err, parser_ok) << parser_getErrorDescription(err);
        EXPECT_EQ(numChunks, 1) << "Incorrect number of chunks";
        EXPECT_EQ_STR(key, "keyA", "Incorrect key")
        EXPECT_EQ_STR(val, "123456", "Incorrect value")

        // Try the same again
        INIT_QUERY_CONTEXT(key, sizeof(key), val, sizeof(val), 0, 4)
        parser_tx_obj.tx_json.query.item_index = 0;
        err = tx_traverse(0, &numChunks);
        EXPECT_EQ(err, parser_ok) << parser_getErrorDescription(err);
        EXPECT_EQ(numChunks, 1) << "Incorrect number of chunks";
        EXPECT_EQ_STR(key, "keyA", "Incorrect key")
        EXPECT_EQ_STR(val, "123456", "Incorrect value")

        // Try last key
        INIT_QUERY_CONTEXT(key, sizeof(key), val, sizeof(val), 0, 4)
        parser_tx_obj.tx_json.query.item_index = 2;
        err = tx_traverse(0, &numChunks);
        EXPECT_EQ(err, parser_ok) << parser_getErrorDescription(err);
        EXPECT_EQ(numChunks, 1) << "Incorrect number of chunks";
        EXPECT_EQ_STR(key, "keyC", "Incorrect key")
        EXPECT_EQ_STR(val, "", "Incorrect value")
    }

    TEST(TxParse, OutOfBoundsSmall) {
        auto transaction = R"({"keyA":"123456", "keyB":"abcdefg"})";

        parser_tx_obj.tx_json.tx = transaction;
        parser_tx_obj.tx_json.flags.cache_valid = false;
        parser_error_t err = JSON_PARSE(&parser_tx_obj.tx_json.json, parser_tx_obj.tx_json.tx);
        ASSERT_EQ(err, parser_ok);

        char key[1000];
        char val[1000];
        uint8_t numChunks;

        INIT_QUERY_CONTEXT(key, sizeof(key), val, sizeof(val), 5, 4)
        err = tx_traverse(0, &numChunks);
        EXPECT_EQ(err, parser_display_page_out_of_range) << "This call should have resulted in a display out of range";

        // We should find it. but later tx_display should fail
        INIT_QUERY_CONTEXT(key, sizeof(key), val, sizeof(val), 0, 4)
        err = tx_traverse(0, &numChunks);
        EXPECT_EQ(err, parser_ok);
        EXPECT_EQ(numChunks, 1) << "Item not found";
    }

    TEST(TxParse, Count_Minimal) {
        auto transaction = R"({"account_number":"3"})";

        parser_tx_obj.tx_json.tx = transaction;
        parser_tx_obj.tx_json.flags.cache_valid = false;
        parser_error_t err = JSON_PARSE(&parser_tx_obj.tx_json.json, parser_tx_obj.tx_json.tx);
        EXPECT_EQ(err, parser_ok);

        uint8_t numItems;
        tx_display_numItems(&numItems);

        EXPECT_EQ(1, numItems) << "Wrong number of items";
    }

    TEST(TxParse, Tx_Chain_not_Allowed) {
        auto transaction = R"({"account_number":"588","chain_id":"0","fee":{"amount":[],"gas":"2000000"},"memo":"TestMemo","msgs":[{"type":"mayachain/MsgSend","value":{"amount":[{"amount":"150000000","denom":"cacao"}],"from_address":"tmaya1c648xgpter9xffhmcqvs7lzd7hxh0prgvr4c73","to_address":"tmaya10xgrknu44d83qr4s4uw56cqxg0hsev5e6sp5nj","test":"test"}}],"sequence":"5"})";

        parser_tx_obj.tx_json.tx = transaction;
        parser_tx_obj.tx_json.flags.cache_valid = false;
        parser_error_t err = JSON_PARSE(&parser_tx_obj.tx_json.json, parser_tx_obj.tx_json.tx);
        EXPECT_EQ(err, parser_ok);

        uint8_t numItems;
        parser_error_t err2 = tx_display_numItems(&numItems);
        EXPECT_EQ(err2, parser_unexpected_chain);
    }

    TEST(TxParse, Tx_Page_Count) {
        auto transaction = R"({"account_number":"588","chain_id":"mayachain","fee":{"amount":[],"gas":"2000000"},"memo":"TestMemo","msgs":[{"type":"mayachain/MsgSend","value":{"amount":[{"amount":"150000000","denom":"cacao"}],"from_address":"tmaya1c648xgpter9xffhmcqvs7lzd7hxh0prgvr4c73","to_address":"tmaya10xgrknu44d83qr4s4uw56cqxg0hsev5e6sp5nj","test":"test"}}],"sequence":"5"})";

        parser_tx_obj.tx_json.tx = transaction;
        parser_tx_obj.tx_json.flags.cache_valid = false;
        const parser_error_t err = JSON_PARSE(&parser_tx_obj.tx_json.json, parser_tx_obj.tx_json.tx);
        EXPECT_EQ(err, parser_ok);

        uint8_t numItems;
        tx_display_numItems(&numItems);
        EXPECT_EQ(6, numItems) << "Wrong number of items";
    }
}
