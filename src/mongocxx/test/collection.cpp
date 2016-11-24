// Copyright 2015 MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "catch.hpp"
#include <mongocxx/test_util/client_helpers.hh>

#include <vector>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/query_exception.hpp>
#include <mongocxx/exception/write_exception.hpp>
#include <mongocxx/insert_many_builder.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pipeline.hpp>
#include <mongocxx/read_concern.hpp>

using namespace bsoncxx::builder::stream;
using namespace mongocxx;

TEST_CASE("A default constructed collection cannot perform operations", "[collection]") {
    instance::current();

    collection c;
    REQUIRE_THROWS_AS(c.name(), mongocxx::logic_error);
}

TEST_CASE("collection copy", "[collection]") {
    instance::current();

    client mongodb_client{uri{}};
    database db = mongodb_client["test"];

    std::string collname{"foo"};
    std::string collname2{"bar"};
    collection coll = db[collname];

    collection coll2{coll};
    collection coll3 = db[collname2];
    coll3 = coll;

    REQUIRE(coll2.name() == stdx::string_view{collname});
    REQUIRE(coll3.name() == stdx::string_view{collname});
}

TEST_CASE("collection renaming", "[collection]") {
    instance::current();

    client mongodb_client{uri{}};
    database db = mongodb_client["test"];

    std::string collname{"mongo_cxx_driver"};
    collection coll = db[collname];
    coll.insert_one({});  // Ensure that the collection exists.

    REQUIRE(coll.name() == stdx::string_view{collname});

    std::string new_name{"mongo_cxx_newname"};
    coll.rename(new_name, true);

    REQUIRE(coll.name() == stdx::string_view{new_name});
}

TEST_CASE("CRUD functionality", "[driver::collection]") {
    instance::current();

    client mongodb_client{uri{}};
    database db = mongodb_client["test"];
    collection coll = db["mongo_cxx_driver"];

    auto case_insensitive_collation = document{} << "locale"
                                                 << "en_US"
                                                 << "strength" << 2 << finalize;

    coll.drop();

    SECTION("insert and read single document", "[collection]") {
        auto b = document{} << "_id" << bsoncxx::oid{} << "x" << 1 << finalize;

        REQUIRE(coll.insert_one(b.view()));

        auto c = document{} << "x" << 1 << finalize;
        REQUIRE(coll.insert_one(c.view()));

        auto cursor = coll.find(b.view());

        std::size_t i = 0;
        for (auto&& x : cursor) {
            REQUIRE(x["_id"].get_oid().value == b.view()["_id"].get_oid().value);
            i++;
        }

        REQUIRE(i == 1);
    }

    SECTION("insert_one returns correct result object", "[collection]") {
        stdx::string_view expected_id{"foo"};

        auto result = coll.insert_one(document{} << "_id" << expected_id << finalize);
        REQUIRE(result);
        REQUIRE(result->result().inserted_count() == 1);
        REQUIRE(result->inserted_id().type() == bsoncxx::type::k_utf8);
        REQUIRE(result->inserted_id().get_utf8().value == expected_id);
    }

    SECTION("insert and read multiple documents", "[collection]") {
        document b1;
        document b2;
        document b3;
        document b4;

        b1 << "_id" << bsoncxx::oid{} << "x" << 1;
        b2 << "x" << 2;
        b3 << "x" << 3;
        b4 << "_id" << bsoncxx::oid{} << "x" << 4;

        std::vector<bsoncxx::document::view> docs{};
        docs.push_back(b1.view());
        docs.push_back(b2.view());
        docs.push_back(b3.view());
        docs.push_back(b4.view());

        auto result = coll.insert_many(docs, options::insert{});
        auto cursor = coll.find({});

        SECTION("result count is correct") {
            REQUIRE(result);
            REQUIRE(result->inserted_count() == 4);
        }

        SECTION("read inserted values with range-for") {
            std::int32_t i = 0;
            for (auto&& x : cursor) {
                i++;
                REQUIRE(x["x"].get_int32() == i);
            }

            REQUIRE(i == 4);
        }

        SECTION("multiple iterators move in lockstep") {
            auto end = cursor.end();
            REQUIRE(cursor.begin() != end);

            auto iter1 = cursor.begin();
            auto iter2 = cursor.begin();
            REQUIRE(iter1 == iter2);
            REQUIRE(*iter1 == *iter2);
            iter1++;
            REQUIRE(iter1 == iter2);
            REQUIRE(iter1 != end);
            REQUIRE(*iter1 == *iter2);
        }
    }

    SECTION("insert_many returns correct result object", "[collection]") {
        document b1;
        document b2;

        b1 << "_id"
           << "foo"
           << "x" << 1;
        b2 << "x" << 2;

        std::vector<bsoncxx::document::view> docs{};
        docs.push_back(b1.view());
        docs.push_back(b2.view());

        auto result = coll.insert_many(docs);

        REQUIRE(result);

        // Verify result->result() is correct:
        REQUIRE(result->result().inserted_count() == 2);

        // Verify result->inserted_count() is correct:
        REQUIRE(result->inserted_count() == 2);

        // Verify result->inserted_ids() is correct:
        auto id_map = result->inserted_ids();
        REQUIRE(id_map[0].type() == bsoncxx::type::k_utf8);
        REQUIRE(id_map[0].get_utf8().value == stdx::string_view{"foo"});
        REQUIRE(id_map[1].type() == bsoncxx::type::k_oid);
        auto second_inserted_doc = coll.find_one(document{} << "x" << 2 << finalize);
        REQUIRE(second_inserted_doc);
        REQUIRE(second_inserted_doc->view()["_id"]);
        REQUIRE(second_inserted_doc->view()["_id"].type() == bsoncxx::type::k_oid);
        REQUIRE(id_map[1].get_oid().value == second_inserted_doc->view()["_id"].get_oid().value);
    }

    SECTION("find with collation", "[collection]") {
        auto b = document{} << "x"
                            << "foo" << finalize;
        REQUIRE(coll.insert_one(b.view()));

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;
        auto find_opts = options::find{}.collation(case_insensitive_collation.view());
        auto cursor = coll.find(predicate.view(), find_opts);
        if (test_util::supports_collation(mongodb_client)) {
            REQUIRE(std::distance(cursor.begin(), cursor.end()) == 1);
        } else {
            REQUIRE_THROWS_AS(std::distance(cursor.begin(), cursor.end()), query_exception);
        }
    }

    SECTION("find_one with collation", "[collection]") {
        auto b = document{} << "x"
                            << "foo" << finalize;
        REQUIRE(coll.insert_one(b.view()));

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;
        auto find_opts = options::find{}.collation(case_insensitive_collation.view());
        if (test_util::supports_collation(mongodb_client)) {
            REQUIRE(coll.find_one(predicate.view(), find_opts));
        } else {
            REQUIRE_THROWS_AS(coll.find_one(predicate.view(), find_opts), query_exception);
        }
    }

    SECTION("insert and update single document", "[collection]") {
        auto b1 = document{} << "_id" << 1 << finalize;

        coll.insert_one(b1.view());

        auto doc = coll.find_one({});
        REQUIRE(doc);
        REQUIRE(doc->view()["_id"].get_int32() == 1);

        document update_doc;
        update_doc << "$set" << open_document << "changed" << true << close_document;

        coll.update_one(b1.view(), update_doc.view());

        auto updated = coll.find_one({});
        REQUIRE(updated);
        REQUIRE(updated->view()["changed"].get_bool() == true);
    }

    SECTION("update_one with collation", "[collection]") {
        auto b = document{} << "x"
                            << "foo" << finalize;
        REQUIRE(coll.insert_one(b.view()));

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;

        document update_doc;
        update_doc << "$set" << open_document << "changed" << true << close_document;

        auto update_opts = options::update{}.collation(case_insensitive_collation.view());
        if (test_util::supports_collation(mongodb_client)) {
            auto result = coll.update_one(predicate.view(), update_doc.view(), update_opts);
            REQUIRE(result);
            REQUIRE(result->modified_count() == 1);
        } else {
            REQUIRE_THROWS_AS(coll.update_one(predicate.view(), update_doc.view(), update_opts),
                              bulk_write_exception);
        }
    }

    SECTION("insert and update multiple documents", "[collection]") {
        auto b1 = document{} << "x" << 1 << finalize;

        coll.insert_one(b1.view());
        coll.insert_one(b1.view());

        auto b2 = document{} << "x" << 2 << finalize;

        coll.insert_one(b2.view());

        REQUIRE(coll.count(b1.view()) == 2);

        document bchanged;
        bchanged << "changed" << true;

        document update_doc;
        update_doc << "$set" << bsoncxx::types::b_document{bchanged};

        coll.update_many(b1.view(), update_doc.view());

        REQUIRE(coll.count(bchanged.view()) == 2);
    }

    SECTION("update_many with collation", "[collection]") {
        auto b = document{} << "x"
                            << "foo" << finalize;
        REQUIRE(coll.insert_one(b.view()));

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;

        document update_doc;
        update_doc << "$set" << open_document << "changed" << true << close_document;

        auto update_opts = options::update{}.collation(case_insensitive_collation.view());
        if (test_util::supports_collation(mongodb_client)) {
            auto result = coll.update_many(predicate.view(), update_doc.view(), update_opts);
            REQUIRE(result);
            REQUIRE(result->modified_count() == 1);
        } else {
            REQUIRE_THROWS_AS(coll.update_many(predicate.view(), update_doc.view(), update_opts),
                              bulk_write_exception);
        }
    }

    SECTION("replace document replaces only one document", "[collection]") {
        document doc;
        doc << "x" << 1;

        coll.insert_one(doc.view());
        coll.insert_one(doc.view());

        REQUIRE(coll.count(doc.view()) == 2);

        document replacement;
        replacement << "x" << 2;

        coll.replace_one(doc.view(), replacement.view());
        auto c = coll.count(doc.view());
        REQUIRE(coll.count(doc.view()) == 1);
    }

    SECTION("non-matching upsert creates document", "[collection]") {
        document b1;
        b1 << "_id" << 1;

        document update_doc;
        update_doc << "$set" << open_document << "changed" << true << close_document;

        options::update options;
        options.upsert(true);

        auto result = coll.update_one(b1.view(), update_doc.view(), options);
        REQUIRE(result->upserted_id());

        auto updated = coll.find_one({});

        REQUIRE(updated);
        REQUIRE(updated->view()["changed"].get_bool() == true);
        REQUIRE(coll.count({}) == (std::int64_t)1);
    }

    SECTION("matching upsert updates document", "[collection]") {
        document b1;
        b1 << "_id" << 1;

        coll.insert_one(b1.view());

        document update_doc;
        update_doc << "$set" << open_document << "changed" << true << close_document;

        options::update options;
        options.upsert(true);

        auto result = coll.update_one(b1.view(), update_doc.view(), options);
        REQUIRE(!(result->upserted_id()));

        auto updated = coll.find_one({});

        REQUIRE(updated);
        REQUIRE(updated->view()["changed"].get_bool() == true);
        REQUIRE(coll.count({}) == 1);
    }

    SECTION("test using an insert_many_builder on this collection", "[collection]") {
        auto doc_value = document{} << "x" << 1 << finalize;
        auto doc_view = doc_value.view();

        insert_many_builder insert_many{options::insert()};
        insert_many(doc_view);
        insert_many(doc_view);
        insert_many(doc_view);

        insert_many.insert(&coll);

        coll.insert_one(document{} << "b" << 1 << finalize);

        REQUIRE(coll.count(doc_view) == 3);
        REQUIRE(coll.count({}) == 4);
    }

    SECTION("count with hint", "[collection]") {
        options::count count_opts;
        count_opts.hint(hint{"index_doesnt_exist"});

        auto doc = document{} << "x" << 1 << finalize;
        coll.insert_one(doc.view());

        if (test_util::get_max_wire_version(mongodb_client) >= 2) {
            REQUIRE_THROWS_AS(coll.count(doc.view(), count_opts), operation_exception);
        } else {
            // Old server versions ignore hint sent with count.
            REQUIRE(1 == coll.count(doc.view(), count_opts));
        }
    }

    SECTION("count with collation", "[collection]") {
        auto doc = document{} << "x"
                              << "foo" << finalize;
        REQUIRE(coll.insert_one(doc.view()));

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;
        auto count_opts = options::count{}.collation(case_insensitive_collation.view());
        if (test_util::supports_collation(mongodb_client)) {
            REQUIRE(coll.count(predicate.view(), count_opts) == 1);
        } else {
            REQUIRE_THROWS_AS(coll.count(predicate.view(), count_opts), query_exception);
        }
    }

    SECTION("document replacement", "[collection]") {
        document b1;
        b1 << "x" << 1;
        coll.insert_one(b1.view());

        document b2;
        b2 << "x" << 2;

        coll.replace_one(b1.view(), b2.view());

        auto replaced = coll.find_one(b2.view());

        REQUIRE(replaced);
        REQUIRE(coll.count({}) == 1);
    }

    SECTION("replace_one with collation", "[collection]") {
        document doc;
        doc << "x"
            << "foo";
        REQUIRE(coll.insert_one(doc.view()));

        document predicate;
        predicate << "x"
                  << "FOO";

        document replacement_doc;
        replacement_doc << "x"
                        << "bar";

        auto update_opts = options::update{}.collation(case_insensitive_collation.view());
        if (test_util::supports_collation(mongodb_client)) {
            auto result = coll.replace_one(predicate.view(), replacement_doc.view(), update_opts);
            REQUIRE(result);
            REQUIRE(result->modified_count() == 1);
        } else {
            REQUIRE_THROWS_AS(
                coll.replace_one(predicate.view(), replacement_doc.view(), update_opts),
                bulk_write_exception);
        }
    }

    SECTION("filtered document delete one works", "[collection]") {
        document b1;
        b1 << "x" << 1;

        coll.insert_one(b1.view());

        document b2;
        b2 << "x" << 2;

        coll.insert_one(b2.view());
        coll.insert_one(b2.view());

        REQUIRE(coll.count({}) == 3);

        coll.delete_one(b2.view());

        REQUIRE(coll.count({}) == (std::int64_t)2);

        auto cursor = coll.find({});

        unsigned seen = 0;
        for (auto&& x : cursor) {
            seen |= x["x"].get_int32();
        }

        REQUIRE(seen == 3);

        coll.delete_one(b2.view());

        REQUIRE(coll.count({}) == 1);

        cursor = coll.find({});

        seen = 0;
        for (auto&& x : cursor) {
            seen |= x["x"].get_int32();
        }

        REQUIRE(seen == 1);

        coll.delete_one(b2.view());

        REQUIRE(coll.count({}) == 1);

        cursor = coll.find({});

        seen = 0;
        for (auto&& x : cursor) {
            seen |= x["x"].get_int32();
        }

        REQUIRE(seen == 1);
    }

    SECTION("delete_one with collation", "[collection]") {
        document b1;
        b1 << "x"
           << "foo";

        REQUIRE(coll.insert_one(b1.view()));

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;

        auto delete_opts = options::delete_options{}.collation(case_insensitive_collation.view());
        if (test_util::supports_collation(mongodb_client)) {
            auto result = coll.delete_one(predicate.view(), delete_opts);
            REQUIRE(result);
            REQUIRE(result->deleted_count() == 1);
        } else {
            REQUIRE_THROWS_AS(coll.delete_one(predicate.view(), delete_opts), bulk_write_exception);
        }
    }

    SECTION("delete many works", "[collection]") {
        document b1;
        b1 << "x" << 1;

        coll.insert_one(b1.view());

        document b2;
        b2 << "x" << 2;

        coll.insert_one(b2.view());
        coll.insert_one(b2.view());

        REQUIRE(coll.count({}) == 3);

        coll.delete_many(b2.view());

        REQUIRE(coll.count({}) == 1);

        auto cursor = coll.find({});

        unsigned seen = 0;
        for (auto&& x : cursor) {
            seen |= x["x"].get_int32();
        }

        REQUIRE(seen == 1);

        coll.delete_many(b2.view());

        REQUIRE(coll.count({}) == 1);

        cursor = coll.find({});

        seen = 0;
        for (auto&& x : cursor) {
            seen |= x["x"].get_int32();
        }

        REQUIRE(seen == 1);
    }

    SECTION("delete_many with collation", "[collection]") {
        document b1;
        b1 << "x"
           << "foo";

        REQUIRE(coll.insert_one(b1.view()));

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;

        auto delete_opts = options::delete_options{}.collation(case_insensitive_collation.view());
        if (test_util::supports_collation(mongodb_client)) {
            auto result = coll.delete_many(predicate.view(), delete_opts);
            REQUIRE(result);
            REQUIRE(result->deleted_count() == 1);
        } else {
            REQUIRE_THROWS_AS(coll.delete_many(predicate.view(), delete_opts),
                              bulk_write_exception);
        }
    }

    SECTION("find works with sort", "[collection]") {
        document b1;
        b1 << "x" << 1;

        document b2;
        b2 << "x" << 2;

        document b3;
        b3 << "x" << 3;

        coll.insert_one(b1.view());
        coll.insert_one(b3.view());
        coll.insert_one(b2.view());

        SECTION("sort ascending") {
            document sort;
            sort << "x" << 1;
            options::find opts{};
            opts.sort(sort.view());

            auto cursor = coll.find({}, opts);

            std::int32_t x = 1;
            for (auto&& doc : cursor) {
                REQUIRE(x == doc["x"].get_int32());
                x++;
            }
        }

        SECTION("sort descending") {
            document sort;
            sort << "x" << -1;
            options::find opts{};
            opts.sort(sort.view());

            auto cursor = coll.find({}, opts);

            std::int32_t x = 3;
            for (auto&& doc : cursor) {
                REQUIRE(x == doc["x"].get_int32());
                x--;
            }
        }
    }

    SECTION("find_one_and_replace works", "[collection]") {
        document b1;
        b1 << "x"
           << "foo";

        coll.insert_one(b1.view());
        coll.insert_one(b1.view());

        REQUIRE(coll.count({}) == 2);

        document criteria;
        document replacement;

        criteria << "x"
                 << "foo";
        replacement << "x"
                    << "bar";

        SECTION("without return replacement returns original") {
            auto doc = coll.find_one_and_replace(criteria.view(), replacement.view());
            REQUIRE(doc);
            REQUIRE(doc->view()["x"].get_utf8().value == stdx::string_view{"foo"});
        }

        SECTION("with return replacement returns new") {
            options::find_one_and_replace options;
            options.return_document(options::return_document::k_after);
            auto doc = coll.find_one_and_replace(criteria.view(), replacement.view(), options);
            REQUIRE(doc);
            REQUIRE(doc->view()["x"].get_utf8().value == stdx::string_view{"bar"});
        }

        SECTION("with collation") {
            options::find_one_and_replace options;
            options.collation(case_insensitive_collation.view());

            document collation_criteria;
            collation_criteria << "x"
                               << "FOO";

            if (test_util::supports_collation(mongodb_client)) {
                auto doc = coll.find_one_and_replace(collation_criteria.view(), replacement.view(),
                                                     options);
                REQUIRE(doc);
                REQUIRE(doc->view()["x"].get_utf8().value == stdx::string_view{"foo"});
            } else {
                REQUIRE_THROWS_AS(coll.find_one_and_replace(collation_criteria.view(),
                                                            replacement.view(), options),
                                  write_exception);
            }
        }

        SECTION("bad criteria returns negative optional") {
            document bad_criteria;
            bad_criteria << "x"
                         << "baz";

            auto doc = coll.find_one_and_replace(bad_criteria.view(), replacement.view());

            REQUIRE(!doc);
        }
    }

    SECTION("find_one_and_update works", "[collection]") {
        document b1;
        b1 << "x"
           << "foo";

        coll.insert_one(b1.view());
        coll.insert_one(b1.view());

        REQUIRE(coll.count({}) == 2);

        document criteria;
        document update;

        criteria << "x"
                 << "foo";
        update << "$set" << open_document << "x"
               << "bar" << close_document;

        SECTION("without return update returns original") {
            auto doc = coll.find_one_and_update(criteria.view(), update.view());

            REQUIRE(doc);

            REQUIRE(doc->view()["x"].get_utf8().value == stdx::string_view{"foo"});
        }

        SECTION("with return update returns new") {
            options::find_one_and_update options;
            options.return_document(options::return_document::k_after);
            auto doc = coll.find_one_and_update(criteria.view(), update.view(), options);
            REQUIRE(doc);
            REQUIRE(doc->view()["x"].get_utf8().value == stdx::string_view{"bar"});
        }

        SECTION("with collation") {
            options::find_one_and_update options;
            options.collation(case_insensitive_collation.view());

            document collation_criteria;
            collation_criteria << "x"
                               << "FOO";

            if (test_util::supports_collation(mongodb_client)) {
                auto doc =
                    coll.find_one_and_update(collation_criteria.view(), update.view(), options);
                REQUIRE(doc);
                REQUIRE(doc->view()["x"].get_utf8().value == stdx::string_view{"foo"});
            } else {
                REQUIRE_THROWS_AS(
                    coll.find_one_and_update(collation_criteria.view(), update.view(), options),
                    write_exception);
            }
        }

        SECTION("bad criteria returns negative optional") {
            document bad_criteria;
            bad_criteria << "x"
                         << "baz";

            auto doc = coll.find_one_and_update(bad_criteria.view(), update.view());

            REQUIRE(!doc);
        }
    }

    SECTION("find_one_and_delete works", "[collection]") {
        document b1;
        b1 << "x"
           << "foo";

        coll.insert_one(b1.view());
        coll.insert_one(b1.view());

        REQUIRE(coll.count({}) == 2);

        document criteria;

        criteria << "x"
                 << "foo";

        SECTION("delete one deletes one and returns it") {
            auto doc = coll.find_one_and_delete(criteria.view());

            REQUIRE(doc);

            REQUIRE(doc->view()["x"].get_utf8().value == stdx::string_view{"foo"});
            REQUIRE(coll.count({}) == 1);
        }

        SECTION("with collation") {
            options::find_one_and_delete options;
            options.collation(case_insensitive_collation.view());

            document collation_criteria;
            collation_criteria << "x"
                               << "FOO";

            if (test_util::supports_collation(mongodb_client)) {
                auto doc = coll.find_one_and_delete(collation_criteria.view(), options);
                REQUIRE(doc);
                REQUIRE(doc->view()["x"].get_utf8().value == stdx::string_view{"foo"});
            } else {
                REQUIRE_THROWS_AS(coll.find_one_and_delete(collation_criteria.view(), options),
                                  write_exception);
            }
        }
    }

    SECTION("aggregation", "[collection]") {
        pipeline pipeline;

        auto get_results = [](cursor&& cursor) {
            std::vector<bsoncxx::document::value> results;
            std::transform(cursor.begin(), cursor.end(), std::back_inserter(results),
                           [](bsoncxx::document::view v) { return bsoncxx::document::value{v}; });
            return results;
        };

        SECTION("lookup") {
            coll.insert_one(document{} << "x" << 0 << finalize);
            coll.insert_one(document{} << "x" << 1 << "y" << 0 << finalize);

            pipeline.lookup(document{} << "from" << coll.name() << "localField"
                                       << "x"
                                       << "foreignField"
                                       << "y"
                                       << "as"
                                       << "z" << finalize);
            // Add a sort to the pipeline, so below tests can make assumptions about result order.
            pipeline.sort(document{} << "x" << 1 << finalize);
            auto cursor = coll.aggregate(pipeline);

            if (test_util::get_max_wire_version(mongodb_client) >= 4) {
                // The server supports lookup().
                auto results = get_results(std::move(cursor));
                REQUIRE(results.size() == 2);
                REQUIRE(!results[0].view()["z"].get_array().value.empty());
                REQUIRE(results[1].view()["z"].get_array().value.empty());
            } else {
                // The server does not support lookup().
                REQUIRE_THROWS_AS(get_results(std::move(cursor)), operation_exception);
            }
        }

        SECTION("match") {
            coll.insert_one(document{} << "x" << 1 << finalize);
            coll.insert_one(document{} << "x" << 1 << finalize);
            coll.insert_one(document{} << "x" << 2 << finalize);

            pipeline.match(document{} << "x" << 1 << finalize);
            auto cursor = coll.aggregate(pipeline);

            auto results = get_results(std::move(cursor));
            REQUIRE(results.size() == 2);
        }
    }

    SECTION("aggregation with collation", "[collection]") {
        document b1;
        b1 << "x"
           << "foo";

        coll.insert_one(b1.view());

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;

        pipeline p;
        p.match(predicate.view());

        auto agg_opts = options::aggregate{}.collation(case_insensitive_collation.view());
        auto results = coll.aggregate(p, agg_opts);

        if (test_util::supports_collation(mongodb_client)) {
            REQUIRE(std::distance(results.begin(), results.end()) == 1);
        } else {
            // The server does not support collation.
            REQUIRE_THROWS_AS(std::distance(results.begin(), results.end()), operation_exception);
        }
    }

    SECTION("distinct works", "[collection]") {
        auto doc1 = document{} << "foo"
                               << "baz"
                               << "garply" << 1 << finalize;
        auto doc2 = document{} << "foo"
                               << "bar"
                               << "garply" << 2 << finalize;
        auto doc3 = document{} << "foo"
                               << "baz"
                               << "garply" << 2 << finalize;
        auto doc4 = document{} << "foo"
                               << "quux"
                               << "garply" << 9 << finalize;

        options::bulk_write bulk_opts;
        bulk_opts.ordered(false);
        bulk_write bulk{bulk_opts};

        bulk.append(model::insert_one{std::move(doc1)});
        bulk.append(model::insert_one{std::move(doc2)});
        bulk.append(model::insert_one{std::move(doc3)});
        bulk.append(model::insert_one{std::move(doc4)});

        coll.bulk_write(bulk);

        auto distinct_results = coll.distinct("foo", {});

        // copy into a vector.
        std::vector<bsoncxx::document::value> results;
        for (auto&& result : distinct_results) {
            results.emplace_back(result);
        }

        REQUIRE(results.size() == std::size_t{1});

        auto res_doc = results[0].view();
        auto values_array = res_doc["values"].get_array().value;

        std::vector<stdx::string_view> distinct_values;
        for (auto&& value : values_array) {
            distinct_values.push_back(value.get_utf8().value);
        }

        const auto assert_contains_one = [&](stdx::string_view val) {
            REQUIRE(std::count(distinct_values.begin(), distinct_values.end(), val) == 1);
        };

        assert_contains_one("baz");
        assert_contains_one("bar");
        assert_contains_one("quux");
    }

    SECTION("distinct with collation", "[collection]") {
        auto doc = document{} << "x"
                              << "foo" << finalize;

        coll.insert_one(doc.view());

        auto predicate = document{} << "x"
                                    << "FOO" << finalize;

        auto distinct_opts = options::distinct{}.collation(case_insensitive_collation.view());

        if (test_util::supports_collation(mongodb_client)) {
            auto distinct_results = coll.distinct("x", predicate.view(), distinct_opts);
            auto iter = distinct_results.begin();
            REQUIRE(iter != distinct_results.end());
            auto result = *iter;
            auto values = result["values"].get_array().value;
            REQUIRE(std::distance(values.begin(), values.end()) == 1);
            REQUIRE(values[0].get_utf8().value == stdx::string_view{"foo"});
        } else {
            // The server does not support collation.
            REQUIRE_THROWS_AS(coll.distinct("x", predicate.view(), distinct_opts),
                              operation_exception);
        }
    }
}

TEST_CASE("read_concern is inherited from parent", "[collection]") {
    client mongo_client{uri{}};
    database db = mongo_client["test"];

    read_concern::level majority = read_concern::level::k_majority;
    read_concern::level local = read_concern::level::k_local;

    read_concern rc{};
    rc.acknowledge_level(majority);
    db.read_concern(rc);

    collection coll = db["rc"];

    SECTION("when parent is a database") {
        REQUIRE(coll.read_concern().acknowledge_level() == read_concern::level::k_majority);
    }

    SECTION("except when read_concern is explicitly set") {
        read_concern set_rc{};
        set_rc.acknowledge_level(read_concern::level::k_local);
        coll.read_concern(set_rc);

        REQUIRE(coll.read_concern().acknowledge_level() == local);
    }
}

TEST_CASE("create_index tests", "[collection]") {
    instance::current();

    client mongodb_client{uri{}};
    database db = mongodb_client["test"];
    collection coll = db["collection"];
    coll.drop();
    coll.insert_one({});  // Ensure that the collection exists.

    SECTION("returns index name") {
        bsoncxx::document::value index = bsoncxx::builder::stream::document{}
                                         << "a" << 1 << bsoncxx::builder::stream::finalize;

        std::string indexName{"myName"};
        options::index options{};
        options.name(indexName);

        auto response = coll.create_index(index.view(), options);
        REQUIRE(response.view()["name"].get_utf8().value == stdx::string_view{indexName});

        bsoncxx::document::value index2 = bsoncxx::builder::stream::document{}
                                          << "b" << 1 << "c" << -1
                                          << bsoncxx::builder::stream::finalize;

        auto response2 = coll.create_index(index2.view(), options::index{});
        REQUIRE(response2.view()["name"].get_utf8().value == stdx::string_view{"b_1_c_-1"});
    }

    SECTION("with collation") {
        bsoncxx::document::value index = bsoncxx::builder::stream::document{}
                                         << "a" << 1 << bsoncxx::builder::stream::finalize;

        auto collation = document{} << "locale"
                                    << "en_US" << finalize;
        options::index options{};
        options.collation(collation.view());

        if (test_util::supports_collation(mongodb_client)) {
            coll.create_index(index.view(), options);

            auto cursor = coll.list_indexes();
            bool found = false;
            for (auto&& doc : cursor) {
                auto name_ele = doc["name"];
                REQUIRE(name_ele);
                REQUIRE(name_ele.type() == bsoncxx::type::k_utf8);
                if (name_ele.get_utf8().value != stdx::string_view{"a_1"}) {
                    continue;
                }
                found = true;
                auto locale_ele = doc["collation"]["locale"];
                REQUIRE(locale_ele);
                REQUIRE(locale_ele.type() == bsoncxx::type::k_utf8);
                REQUIRE(locale_ele.get_utf8() == collation.view()["locale"].get_utf8());
            }
            REQUIRE(found);
        } else {
            REQUIRE_THROWS_AS(coll.create_index(index.view(), options), operation_exception);
        }
    }
}

TEST_CASE("regressions", "CXX-986") {
    instance::current();
    mongocxx::uri mongo_uri{"mongodb://non-existent-host.invalid/"};
    mongocxx::client client{mongo_uri};
    REQUIRE_THROWS(client.database("irrelevant")["irrelevant"].find_one_and_update(
        document{} << "irrelevant" << 1 << finalize, document{} << "irrelevant" << 2 << finalize));
}
