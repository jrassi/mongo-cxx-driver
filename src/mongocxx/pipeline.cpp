// Copyright 2014 MongoDB Inc.
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

#include <mongocxx/pipeline.hpp>

#include <bsoncxx/stdx/make_unique.hpp>
#include <mongocxx/private/pipeline.hh>
#include <mongocxx/stdx.hpp>

#include <mongocxx/config/private/prelude.hh>

namespace mongocxx {
MONGOCXX_INLINE_NAMESPACE_BEGIN

using namespace bsoncxx::builder::stream;

pipeline::pipeline() : _impl(stdx::make_unique<impl>()) {
}

pipeline::pipeline(pipeline&&) noexcept = default;
pipeline& pipeline::operator=(pipeline&&) noexcept = default;
pipeline::~pipeline() = default;

pipeline& pipeline::group(bsoncxx::document::view_or_value group_args) {
    _impl->sink() << open_document << "$group" << group_args << close_document;
    return *this;
}

pipeline& pipeline::limit(std::int32_t limit) {
    _impl->sink() << open_document << "$limit" << limit << close_document;
    return *this;
}

pipeline& pipeline::lookup(bsoncxx::document::view_or_value lookup_args) {
    _impl->sink() << open_document << "$lookup" << lookup_args << close_document;
    return *this;
}

pipeline& pipeline::match(bsoncxx::document::view_or_value filter) {
    _impl->sink() << open_document << "$match" << filter << close_document;
    return *this;
}

pipeline& pipeline::out(std::string collection_name) {
    _impl->sink() << open_document << "$out" << collection_name << close_document;
    return *this;
}

pipeline& pipeline::project(bsoncxx::document::view_or_value projection) {
    _impl->sink() << open_document << "$project" << projection << close_document;
    return *this;
}

pipeline& pipeline::redact(bsoncxx::document::view_or_value restrictions) {
    _impl->sink() << open_document << "$redact" << restrictions << close_document;
    return *this;
}

pipeline& pipeline::sample(std::int32_t size) {
    _impl->sink() << open_document << "$sample" << open_document << "size" << size << close_document
                  << close_document;
    return *this;
}

pipeline& pipeline::skip(std::int32_t docs_to_skip) {
    _impl->sink() << open_document << "$skip" << docs_to_skip << close_document;
    return *this;
}

pipeline& pipeline::sort(bsoncxx::document::view_or_value ordering) {
    _impl->sink() << open_document << "$sort" << ordering << close_document;
    return *this;
}

pipeline& pipeline::unwind(std::string field_name) {
    _impl->sink() << open_document << "$unwind" << field_name << close_document;
    return *this;
}

bsoncxx::document::view pipeline::view() const {
    return _impl->view();
}

bsoncxx::array::view pipeline::view_array() const {
    return _impl->view_array();
}

MONGOCXX_INLINE_NAMESPACE_END
}  // namespace mongocxx
