#pragma once

#include "RawIron/DataSchema/Allowlist.h"
#include "RawIron/DataSchema/CollectionConstraints.h"
#include "RawIron/DataSchema/Coercion.h"
#include "RawIron/DataSchema/ColorParse.h"
#include "RawIron/DataSchema/DocumentHeader.h"
#include "RawIron/DataSchema/DistinctStrings.h"
#include "RawIron/DataSchema/IdFormat.h"
#include "RawIron/DataSchema/Migration.h"
#include "RawIron/DataSchema/ObjectShape.h"
#include "RawIron/DataSchema/ParseStrict.h"
#include "RawIron/DataSchema/PathNormalize.h"
#include "RawIron/DataSchema/PrimitiveChecks.h"
#include "RawIron/DataSchema/ReferenceIntegrity.h"
#include "RawIron/DataSchema/Refinement.h"
#include "RawIron/DataSchema/ScalarConstraints.h"
#include "RawIron/DataSchema/SchemaRegistry.h"
#include "RawIron/DataSchema/StringConstraints.h"
#include "RawIron/DataSchema/StringPattern.h"
#include "RawIron/DataSchema/ValidationReport.h"

namespace RawIron::DataSchema {

using ri::data::schema::DocumentHeader;
using ri::data::schema::MigrationEdge;
using ri::data::schema::MigrationRegistry;
using ri::data::schema::ObjectFieldDoc;
using ri::data::schema::ObjectShape;
using ri::data::schema::ParseDocumentHeader;
using ri::data::schema::SchemaId;
using ri::data::schema::SchemaIdLess;
using ri::data::schema::CollectDeclaredObjectKeys;
using ri::data::schema::SchemaRegistry;
using ri::data::schema::UnknownKeyPolicy;
using ri::data::schema::ValidateObjectShape;
using ri::validate::IssueCode;
using ri::validate::IssueCodeLabel;
using ri::validate::IsFiniteNumber;
using ri::validate::MergeReports;
using ri::validate::NormalizePathSeparators;
using ri::validate::RefinementFn;
using ri::validate::RequireFiniteDouble;
using ri::validate::RunRefinements;
using ri::validate::SafeParseResult;
using ri::validate::TryCoerceBool;
using ri::validate::TryCoerceDouble;
using ri::validate::TryCoerceInt32;
using ri::validate::TryParseColorRgba8;
using ri::validate::UnwrapOrThrow;
using ri::validate::ValidateAllowedString;
using ri::validate::ValidateAsciiIdentifier;
using ri::validate::ValidateHexString;
using ri::validate::ValidateIso8601UtcTimestampString;
using ri::validate::ValidateUuidString;
using ri::validate::ValidateCollectionSize;
using ri::validate::ValidateDistinctStrings;
using ri::validate::ValidateEachObjectKeyMatchesPattern;
using ri::validate::ValidateDoubleInRange;
using ri::validate::ValidateIdsInTable;
using ri::validate::ValidateInt32InRange;
using ri::validate::ValidateRegexMatch;
using ri::validate::ValidateStringLength;
using ri::validate::ValidationIssue;
using ri::validate::ValidationReport;

} // namespace RawIron::DataSchema
