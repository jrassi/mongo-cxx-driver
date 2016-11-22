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

#pragma once

#include <bsoncxx/document/view_or_value.hpp>
#include <bsoncxx/stdx/optional.hpp>
#include <mongocxx/stdx.hpp>

#include <mongocxx/config/prelude.hpp>

namespace mongocxx {
MONGOCXX_INLINE_NAMESPACE_BEGIN

///
/// Class representing criteria for document validation, to be applied to a collection.
///
/// @see https://docs.mongodb.org/manual/core/document-validation/
///
class MONGOCXX_API validation_criteria {
   public:
    ///
    /// Sets a validation rule for this validation object.
    ///
    /// @param rule
    ///   Document representing a validation rule.
    ///
    validation_criteria& rule(bsoncxx::document::view_or_value rule);

    ///
    /// Gets the validation rule for this validation object.
    ///
    /// @return
    ///   Document representing a validation rule.
    ///
    const stdx::optional<bsoncxx::document::view_or_value>& rule() const;

    ///
    /// A class to represent the different validation level options.
    ///
    enum class validation_level {
        // Disable validation entirely.
        k_off,

        // Apply validation rules to inserts, and apply validation rules to updates only if the
        // document to be updated already fulfills the validation criteria.
        k_moderate,

        // Apply validation rules to all inserts and updates.
        k_strict,
    };

    ///
    /// Sets a validation level.
    ///
    /// @param level
    ///   An enumerated validation level.
    ///
    validation_criteria& level(validation_level level);

    ///
    /// Gets the validation level.
    ///
    /// @return
    ///   A validation level, "off," "strict," or "moderate."
    ///
    const stdx::optional<validation_level>& level() const;

    ///
    /// A class to represent the different validation action options.
    ///
    enum class validation_action {
        // Reject any insertion or update that violates the validation criteria.
        k_error,

        // Log any violations of the validation criteria, but allow the insertion or update to
        // proceed.
        k_warn,
    };

    ///
    /// Sets a validation action to run when documents failing validation are inserted or modified.
    ///
    /// @param action
    ///   An enumerated validation action.
    ///
    validation_criteria& action(validation_action action);

    ///
    /// Gets the validation action to run when documents failing validation are inserted or
    /// modified.
    ///
    /// @return
    ///   A validation action, either "error" or "warn."
    ///
    const stdx::optional<validation_action>& action() const;

    ///
    /// Returns a bson document representing this set of validation criteria.
    ///
    /// @deprecated
    ///   This method is deprecated. To determine which options are set on this object, use the
    ///   provided accessors instead.
    ///
    /// @return Validation criteria, as a document.
    ///
    bsoncxx::document::value to_document() const;

    ///
    /// @deprecated
    ///   This method is deprecated. To determine which options are set on this object, use the
    ///   provided accessors instead.
    ///
    MONGOCXX_INLINE operator bsoncxx::document::value() const;

   private:
    stdx::optional<bsoncxx::document::view_or_value> _rule;
    stdx::optional<validation_level> _level;
    stdx::optional<validation_action> _action;
};

MONGOCXX_INLINE validation_criteria::operator bsoncxx::document::value() const {
    return to_document();
}

MONGOCXX_INLINE_NAMESPACE_END
}  // namespace mongocxx

#include <mongocxx/config/postlude.hpp>
