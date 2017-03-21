// Copyright 2017 MongoDB Inc.
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

// TODO CXX-1234: Replace generic exceptions in GridFS with appropriate specific ones

#include <mongocxx/gridfs/bucket.hpp>

#include <exception>
#include <string>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#include <bsoncxx/stdx/optional.hpp>
#include <mongocxx/exception/error_code.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/gridfs/private/bucket.hh>
#include <mongocxx/options/delete.hpp>
#include <mongocxx/options/index.hpp>
#include <mongocxx/stdx.hpp>

#include <mongocxx/config/private/prelude.hh>

namespace mongocxx {
MONGOCXX_INLINE_NAMESPACE_BEGIN
namespace gridfs {

bucket::bucket(const database& db, const options::gridfs::bucket& options) {
    std::string bucket_name = "fs";
    if (auto name = options.bucket_name()) {
        bucket_name = *name;
    }

    if (bucket_name.empty()) {
        throw std::exception{};
    }

    std::size_t default_chunk_size_bytes = 255 * 1024;
    if (auto chunk_size_bytes = options.chunk_size_bytes()) {
        default_chunk_size_bytes = *chunk_size_bytes;
    }

    collection chunks = db[bucket_name + ".chunks"];
    collection files = db[bucket_name + ".files"];

    bool indexes_created = false;

    _impl = stdx::make_unique<impl>(std::move(bucket_name),
                                    default_chunk_size_bytes,
                                    std::move(chunks),
                                    std::move(files),
                                    indexes_created);

    if (auto read_concern = options.read_concern()) {
        _get_impl().files.read_concern(*read_concern);
        _get_impl().chunks.read_concern(*read_concern);
    }

    if (auto read_preference = options.read_preference()) {
        _get_impl().files.read_preference(*read_preference);
        _get_impl().chunks.read_preference(*read_preference);
    }

    if (auto write_concern = options.write_concern()) {
        _get_impl().files.write_concern(*write_concern);
        _get_impl().chunks.write_concern(*write_concern);
    }
}

bucket::bucket() noexcept = default;
bucket::bucket(bucket&&) noexcept = default;
bucket& bucket::operator=(bucket&&) noexcept = default;
bucket::~bucket() = default;

bucket::operator bool() const noexcept {
    return static_cast<bool>(_impl);
}

bucket::bucket(const bucket& b) {
    if (b) {
        _impl = stdx::make_unique<impl>(b._get_impl());
    }
}

bucket& bucket::operator=(const bucket& b) {
    if (b) {
        _impl = stdx::make_unique<impl>(b._get_impl());
    }
    return *this;
}

uploader bucket::open_upload_stream(stdx::string_view filename,
                                    const options::gridfs::upload& options) {
    auto id = bsoncxx::types::value{bsoncxx::types::b_oid{}};
    return open_upload_stream_with_id(id, filename, options);
}

uploader bucket::open_upload_stream_with_id(bsoncxx::types::value id,
                                            stdx::string_view filename,
                                            const options::gridfs::upload& options) {
    std::size_t chunk_size_bytes = _get_impl().default_chunk_size_bytes;

    if (auto chunk_size = options.chunk_size_bytes()) {
        chunk_size_bytes = *chunk_size;
    }

    create_indexes_if_nonexistent();

    return uploader{id,
                    filename,
                    _get_impl().files,
                    _get_impl().chunks,
                    chunk_size_bytes,
                    std::move(options.metadata())};
};

result::gridfs::upload bucket::upload_from_stream(stdx::string_view filename,
                                                  std::istream* source,
                                                  const options::gridfs::upload& options) {
    auto id = bsoncxx::types::value{bsoncxx::types::b_oid{}};
    upload_from_stream_with_id(id, filename, source, options);

    return id;
}

void bucket::upload_from_stream_with_id(bsoncxx::types::value id,
                                        stdx::string_view filename,
                                        std::istream* source,
                                        const options::gridfs::upload& options) {
    uploader upload_stream = open_upload_stream_with_id(id, filename, options);
    std::size_t chunk_size = upload_stream.chunk_size();
    std::unique_ptr<std::uint8_t[]> buffer = stdx::make_unique<std::uint8_t[]>(chunk_size);

    while (!source->eof()) {
        source->read(reinterpret_cast<char*>(buffer.get()), chunk_size);
        upload_stream.write(source->gcount(), buffer.get());
    }

    upload_stream.close();
}

downloader bucket::open_download_stream(bsoncxx::types::value id) {
    using namespace bsoncxx;

    builder::basic::document files_filter;
    files_filter.append(builder::basic::kvp("_id", id));

    auto files_doc = _get_impl().files.find_one(files_filter.extract());

    if (!files_doc) {
        // TODO CXX-1234: Replace generic exceptions in GridFS with appropriate specific ones
        throw std::exception{};
    }

    auto files_doc_view = files_doc->view();

    if (!files_doc_view["length"] || (files_doc_view["length"].type() != type::k_int64 &&
                                      files_doc_view["length"].type() != type::k_int32)) {
        // TODO CXX-1234: Replace generic exceptions in GridFS with appropriate specific ones
        throw std::exception{};
    }

    auto length = files_doc_view["length"];

    if ((length.type() == type::k_int64 && !length.get_int64().value) ||
        (length.type() == type::k_int32 && !length.get_int32().value)) {
        return downloader{stdx::nullopt, *files_doc};
    }

    builder::basic::document chunks_filter;
    chunks_filter.append(builder::basic::kvp("files_id", id));

    builder::basic::document chunks_sort;
    chunks_sort.append(builder::basic::kvp("n", 1));

    options::find chunks_options;
    chunks_options.sort(chunks_sort.extract());

    auto cursor = _get_impl().chunks.find(chunks_filter.extract(), chunks_options);

    return downloader{std::move(cursor), *files_doc};
}

void bucket::download_to_stream(bsoncxx::types::value id, std::ostream* destination) {
    downloader download_stream = open_download_stream(id);
    std::size_t chunk_size = download_stream.chunk_size();
    std::unique_ptr<std::uint8_t[]> buffer = stdx::make_unique<std::uint8_t[]>(chunk_size);
    std::size_t bytes_read;

    while ((bytes_read = download_stream.read(chunk_size, buffer.get())) != 0) {
        destination->write(reinterpret_cast<char*>(buffer.get()), bytes_read);
    }

    download_stream.close();
}

void bucket::delete_file(bsoncxx::types::value id) {
    using namespace bsoncxx;

    builder::basic::document files_builder;
    files_builder.append(builder::basic::kvp("_id", id));

    try {
        if (auto result = _get_impl().files.delete_one(files_builder.extract())) {
            if (result->deleted_count() == 0) {
                // TODO CXX-1234: Replace generic exceptions in GridFS with appropriate specific
                // ones
                throw std::exception{};
            }
        }

        builder::basic::document chunks_builder;
        chunks_builder.append(builder::basic::kvp("files_id", id));
        document::value chunks_filter = chunks_builder.extract();

        _get_impl().chunks.delete_many(chunks_filter.view());
    } catch (const std::exception& e) {
        // TODO CXX-1234: Replace generic exceptions in GridFS with appropriate specific ones
        throw std::exception{};
    }
}

cursor bucket::find(bsoncxx::document::view_or_value filter, const options::find& options) {
    return _get_impl().files.find(filter, options);
}

stdx::string_view bucket::bucket_name() const {
    return _get_impl().bucket_name;
}

void bucket::create_indexes_if_nonexistent() {
    bsoncxx::builder::basic::document filter;
    filter.append(bsoncxx::builder::basic::kvp("_id", 1));

    auto find_options =
        options::find{}.projection(filter.view()).read_preference(read_preference{});

    if (_get_impl().indexes_created || _get_impl().files.find_one({}, find_options)) {
        return;
    }

    bsoncxx::builder::basic::document files_index;
    files_index.append(bsoncxx::builder::basic::kvp("filename", 1));
    files_index.append(bsoncxx::builder::basic::kvp("uploadDate", 1));

    _get_impl().files.create_index(files_index.extract());

    bsoncxx::builder::basic::document chunks_index;
    chunks_index.append(bsoncxx::builder::basic::kvp("files_id", 1));
    chunks_index.append(bsoncxx::builder::basic::kvp("n", 1));

    options::index chunks_index_options;
    chunks_index_options.unique(true);

    _get_impl().chunks.create_index(chunks_index.extract(), chunks_index_options);

    _get_impl().indexes_created = true;
}

const bucket::impl& bucket::_get_impl() const {
    if (!_impl) {
        throw logic_error{error_code::k_invalid_gridfs_bucket_object};
    }
    return *_impl;
}

bucket::impl& bucket::_get_impl() {
    auto cthis = const_cast<const bucket*>(this);
    return const_cast<bucket::impl&>(cthis->_get_impl());
}

}  // namespace gridfs
MONGOCXX_INLINE_NAMESPACE_END
}  // namespace mongocxx
