# Copyright 2016 MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# TODO: Figure out how to link the tests against shared library, in the case where the user doesn't
# build the static library.  Using "target_link_libraries(test_bson bsoncxx)" gives errors like
# these:
#
#   Undefined symbols for architecture x86_64:
#     "bsoncxx::v_noabi::itoa::itoa(unsigned int)", referenced from:
#           ____C_A_T_C_H____T_E_S_T____18() in bson_util_itoa.cpp.o
#
# This is because certain tests (like bson_util_itoa.cpp) currently rely on private symbols (like
# bsoncxx::itoa), which aren't exported by the shared library.

if (NOT BUILD_SHARED_LIBS)
    include_directories(
        ${CMAKE_SOURCE_DIR}/src/third_party/catch/include
    )

    add_executable(test_bson
        ${THIRD_PARTY_SOURCE_DIR}/catch/main.cpp
        bson_b_date.cpp
        bson_builder.cpp
        bson_get_values.cpp
        bson_util_itoa.cpp
        bson_validate.cpp
        json.cpp
        view_or_value.cpp
    )

    target_link_libraries(test_bson bsoncxx)
    add_test(bson test_bson)
endif()
