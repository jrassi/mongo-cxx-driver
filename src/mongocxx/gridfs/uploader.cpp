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

#include <mongocxx/gridfs/uploader.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <iomanip>
#include <ios>
#include <sstream>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/exception/error_code.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/gridfs/private/uploader.hh>

#include <mongocxx/config/private/prelude.hh>

namespace {
std::size_t chunks_collection_documents_max_length(std::size_t chunk_size) {
    // 16 * 1000 * 1000 is used instead of 16 * 1024 * 1024 to ensure that the command document sent
    // to the server has space for the other fields.
    return 16 * 1000 * 1000 / chunk_size;
}
}  // namespace

namespace mongocxx {
MONGOCXX_INLINE_NAMESPACE_BEGIN
namespace gridfs {

uploader::uploader(bsoncxx::types::value id,
                   stdx::string_view filename,
                   collection files,
                   collection chunks,
                   std::size_t chunk_size,
                   stdx::optional<bsoncxx::document::view_or_value> metadata)
    : _impl{stdx::make_unique<impl>(id,
                                    filename,
                                    files,
                                    chunks,
                                    chunk_size,
                                    metadata ? stdx::make_optional<bsoncxx::document::value>(
                                                   bsoncxx::document::value{metadata->view()})
                                             : stdx::nullopt)} {}

uploader::uploader() noexcept = default;
uploader::uploader(uploader&&) noexcept = default;
uploader& uploader::operator=(uploader&&) noexcept = default;
uploader::~uploader() = default;

uploader::operator bool() const noexcept {
    return static_cast<bool>(_impl);
}

void uploader::write(std::size_t length, const std::uint8_t* bytes) {
    if (_get_impl().closed) {
        throw std::exception{};
    }

    while (length > 0) {
        std::size_t buffer_free_space = _get_impl().chunk_size - _get_impl().buffer_off;

        if (buffer_free_space == 0) {
            finish_chunk();
        }

        std::size_t length_written = std::min(length, buffer_free_space);
        std::memcpy(&_get_impl().buffer.get()[_get_impl().buffer_off], bytes, length_written);
        bytes = &bytes[length_written];
        _get_impl().buffer_off += length_written;
        length -= length_written;
    }
}

result::gridfs::upload uploader::close() {
    using bsoncxx::builder::basic::kvp;

    if (_get_impl().closed) {
        throw std::exception{};
    }

    _get_impl().closed = true;

    bsoncxx::builder::basic::document file;

    std::int64_t bytes_uploaded = _get_impl().chunks_written * _get_impl().chunk_size;
    std::int64_t leftover = _get_impl().buffer_off;

    finish_chunk();
    flush_chunks();

    file.append(kvp("_id", _get_impl().result.id()));
    file.append(kvp("length", bytes_uploaded + leftover));
    file.append(kvp("chunkSize", _get_impl().chunk_size));
    file.append(kvp("uploadDate", bsoncxx::types::b_date{std::chrono::system_clock::now()}));

    md5_byte_t md5_hash_array[16];
    md5_finish(&_get_impl().md5, md5_hash_array);

    std::stringstream md5_hash;

    for (auto i = 0; i < 16; ++i) {
        md5_hash << std::setfill('0') << std::setw(2) << std::hex
                 << static_cast<int>(md5_hash_array[i]);
    }

    file.append(kvp("md5", md5_hash.str()));
    file.append(kvp("filename", _get_impl().filename));

    if (_get_impl().metadata) {
        file.append(kvp("metadata", *_get_impl().metadata));
    }

    _get_impl().files.insert_one(file.extract());

    return _get_impl().result;
}

void uploader::abort() {
    if (_get_impl().closed) {
        throw std::exception{};
    }

    _get_impl().closed = true;

    bsoncxx::builder::basic::document filter;
    filter.append(bsoncxx::builder::basic::kvp("files_id", _get_impl().result.id()));

    _get_impl().chunks.delete_many(filter.extract());
}

std::int32_t uploader::chunk_size() const {
    return _get_impl().chunk_size;
}

void uploader::finish_chunk() {
    using bsoncxx::builder::basic::kvp;

    if (!_get_impl().buffer_off) {
        return;
    }

    bsoncxx::builder::basic::document chunk;

    std::size_t bytes_in_chunk = _get_impl().buffer_off;

    chunk.append(kvp("files_id", _get_impl().result.id()));
    chunk.append(kvp("n", static_cast<std::int32_t>(_get_impl().chunks_written)));

    ++_get_impl().chunks_written;

    bsoncxx::types::b_binary data{bsoncxx::binary_sub_type::k_binary,
                                  static_cast<std::uint32_t>(bytes_in_chunk),
                                  _get_impl().buffer.get()};

    md5_append(&_get_impl().md5, _get_impl().buffer.get(), bytes_in_chunk);

    chunk.append(kvp("data", data));
    _get_impl().chunks_collection_documents.push_back(chunk.extract());

    // To reduce the number of calls to the server, chunks are sent in batches rather than each one
    // being sent immediately upon being written.
    if (_get_impl().chunks_collection_documents.size() >=
        chunks_collection_documents_max_length(_get_impl().chunk_size)) {
        flush_chunks();
    }

    _get_impl().buffer_off = 0;
}

void uploader::flush_chunks() {
    if (_get_impl().chunks_collection_documents.empty()) {
        return;
    }

    _get_impl().chunks.insert_many(_get_impl().chunks_collection_documents);
    _get_impl().chunks_collection_documents.clear();
}

const uploader::impl& uploader::_get_impl() const {
    if (!_impl) {
        throw logic_error{error_code::k_invalid_gridfs_uploader_object};
    }
    return *_impl;
}

uploader::impl& uploader::_get_impl() {
    auto cthis = const_cast<const uploader*>(this);
    return const_cast<uploader::impl&>(cthis->_get_impl());
}

}  // namespace gridfs
MONGOCXX_INLINE_NAMESPACE_END
}  // namespace mongocxx
