//
// Copyright 2019 ZetaSQL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "zetasql/public/functions/math.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "zetasql/base/logging.h"
#include "zetasql/common/float_margin.h"
#include "zetasql/base/testing/status_matchers.h"
#include "zetasql/compliance/functions_testlib.h"
#include "zetasql/public/numeric_value.h"
#include "zetasql/public/type.h"
#include "zetasql/public/type.pb.h"
#include "zetasql/public/value.h"
#include "zetasql/testing/test_function.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include "zetasql/base/status.h"

namespace zetasql {
namespace functions {

template <typename T>
inline T GetDummyValue() {
  return 0xDEADBEEF;
}

template <>
inline NumericValue GetDummyValue<NumericValue>() {
  return NumericValue(0xDEADBEEFll);
}

template <typename T>
void CompareResult(const QueryParamsWithResult& param,
                   const absl::Status& actual_status, T actual_value) {
  const Value& expected = param.result();
  if (param.status().ok()) {
    EXPECT_EQ(absl::OkStatus(), actual_status);
    ASSERT_EQ(expected.type_kind(), Value::MakeNull<T>().type_kind());
    if (isnan(expected.Get<T>())) {
      EXPECT_TRUE(isnan(actual_value)) << actual_value;
    } else if (isinf(expected.Get<T>())) {
      EXPECT_EQ(expected.Get<T>(), actual_value);
    } else if (std::numeric_limits<T>::is_integer ||
               param.float_margin().IsExactEquality()) {
      EXPECT_EQ(expected.Get<T>(), actual_value);
    } else {
      EXPECT_TRUE(param.float_margin().Equal(expected.Get<T>(), actual_value))
          << param.float_margin().PrintError(expected.Get<T>(), actual_value);
    }
  } else {
    // Check for the first parameter in the error message.
    EXPECT_THAT(
        actual_status,
        ::zetasql_base::testing::StatusIs(
            absl::StatusCode::kOutOfRange,
            ::testing::HasSubstr(absl::StrCat(param.param(0).Get<T>()))));
  }
}

template <>
void CompareResult<NumericValue>(
    const QueryParamsWithResult& param,
    const absl::Status& actual_status, NumericValue actual_value) {
  // This assumes that the value is stored under NUMERIC feature set but
  // this should work with the default feature set too.
  const QueryParamsWithResult::Result& expected =
      param.results().begin()->second;
  if (expected.status.ok()) {
    EXPECT_EQ(absl::OkStatus(), actual_status);
    ASSERT_EQ(expected.result.type_kind(),
              Value::MakeNull<NumericValue>().type_kind());
    EXPECT_EQ(expected.result.Get<NumericValue>(), actual_value);
  } else {
    // Check for the first parameter in the error message.
    EXPECT_THAT(actual_status,
                ::zetasql_base::testing::StatusIs(
                    absl::StatusCode::kOutOfRange,
                    ::testing::HasSubstr(
                        param.param(0).Get<NumericValue>().ToString())));
  }
}

template <>
void CompareResult<bool>(const QueryParamsWithResult& param,
                         const absl::Status& actual_status, bool actual_value) {
  const Value& expected = param.result();
  if (param.status().ok()) {
    EXPECT_EQ(absl::OkStatus(), actual_status);
    ASSERT_EQ(expected.type_kind(), Value::MakeNull<bool>().type_kind());
    EXPECT_EQ(expected.Get<bool>(), actual_value);
  } else {
    // Check for the first parameter in the error message.
    EXPECT_THAT(
        actual_status,
        ::zetasql_base::testing::StatusIs(
            absl::StatusCode::kOutOfRange,
            ::testing::HasSubstr(absl::StrCat(param.param(0).Get<bool>()))));
  }
}

template <typename InType, typename OutType>
void TestUnaryFunction(const QueryParamsWithResult& param,
                       bool (*function)(InType, OutType*,
                           absl::Status* error)) {
  CHECK_EQ(1, param.num_params());
  const Value& input1 = param.param(0);
  if (input1.is_null()) {
    return;
  }

  OutType out = GetDummyValue<OutType>();
  absl::Status status;  // actual status
  function(input1.Get<InType>(), &out, &status);
  return CompareResult(param, status, out);
}

template <typename InType1, typename InType2, typename OutType>
void TestBinaryFunction(const QueryParamsWithResult& param,
                        bool (*function)(InType1, InType2, OutType*,
                            absl::Status* error)) {
  CHECK_EQ(2, param.num_params());
  const Value& input1 = param.param(0);
  const Value& input2 = param.param(1);
  if (input1.is_null() || input2.is_null()) {
    return;
  }

  OutType out = GetDummyValue<OutType>();
  absl::Status status;  // actual status
  function(input1.Get<InType1>(), input2.Get<InType2>(), &out, &status);
  return CompareResult(param, status, out);
}


typedef testing::TestWithParam<FunctionTestCall> MathTemplateTest;
TEST_P(MathTemplateTest, Testlib) {
  const FunctionTestCall& param = GetParam();
  const std::string& function = param.function_name;
  if (function == "abs") {
    switch (param.params.GetResultType()->kind()) {
      case TYPE_INT32:
        return TestUnaryFunction(param.params, &Abs<int32_t>);
      case TYPE_INT64:
        return TestUnaryFunction(param.params, &Abs<int64_t>);
      case TYPE_UINT32:
        return TestUnaryFunction(param.params, &Abs<uint32_t>);
      case TYPE_UINT64:
        return TestUnaryFunction(param.params, &Abs<uint64_t>);
      case TYPE_FLOAT:
        return TestUnaryFunction(param.params, &Abs<float>);
      case TYPE_DOUBLE:
        return TestUnaryFunction(param.params, &Abs<double>);
      case TYPE_NUMERIC:
        return TestUnaryFunction(param.params, &Abs<NumericValue>);
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "sign") {
    switch (param.params.GetResultType()->kind()) {
      case TYPE_INT32:
        return TestUnaryFunction(param.params, &Sign<int32_t>);
      case TYPE_INT64:
        return TestUnaryFunction(param.params, &Sign<int64_t>);
      case TYPE_UINT32:
        return TestUnaryFunction(param.params, &Sign<uint32_t>);
      case TYPE_UINT64:
        return TestUnaryFunction(param.params, &Sign<uint64_t>);
      case TYPE_FLOAT:
        return TestUnaryFunction(param.params, &Sign<float>);
      case TYPE_DOUBLE:
        return TestUnaryFunction(param.params, &Sign<double>);
      case TYPE_NUMERIC:
        return TestUnaryFunction(param.params, &Sign<NumericValue>);
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "is_inf") {
    switch (param.params.param(0).type_kind()) {
      case TYPE_FLOAT:
        return TestUnaryFunction(param.params, &IsInf<float>);
      case TYPE_DOUBLE:
        return TestUnaryFunction(param.params, &IsInf<double>);
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "is_nan") {
    switch (param.params.param(0).type_kind()) {
      case TYPE_FLOAT:
        return TestUnaryFunction(param.params, &IsNan<float>);
      case TYPE_DOUBLE:
        return TestUnaryFunction(param.params, &IsNan<double>);
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "ieee_divide") {
    switch (param.params.param(0).type_kind()) {
      case TYPE_FLOAT:
        return TestBinaryFunction(param.params, &IeeeDivide<float>);
      case TYPE_DOUBLE:
        return TestBinaryFunction(param.params, &IeeeDivide<double>);
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "sqrt") {
    return TestUnaryFunction(param.params, &Sqrt<double>);
  } else if (function == "pow" || function == "power") {
    switch (param.params.param(0).type_kind()) {
      case TYPE_DOUBLE:
        return TestBinaryFunction(param.params, &Pow<double>);
      case TYPE_NUMERIC:
        return TestBinaryFunction(param.params, &Pow<NumericValue>);
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "exp") {
    return TestUnaryFunction(param.params, &Exp<double>);
  } else if (function == "ln") {
    return TestUnaryFunction(param.params, &NaturalLogarithm<double>);
  } else if (function == "log") {
    if (param.params.num_params() == 1) {
      return TestUnaryFunction(param.params, &NaturalLogarithm<double>);
    } else {
      return TestBinaryFunction(param.params, &Logarithm<double>);
    }
  } else if (function == "log10") {
    return TestUnaryFunction(param.params, &DecimalLogarithm<double>);
  } else if (function == "cos") {
    return TestUnaryFunction(param.params, &Cos<double>);
  } else if (function == "acos") {
    return TestUnaryFunction(param.params, &Acos<double>);
  } else if (function == "cosh") {
    return TestUnaryFunction(param.params, &Cosh<double>);
  } else if (function == "acosh") {
    return TestUnaryFunction(param.params, &Acosh<double>);
  } else if (function == "sin") {
    return TestUnaryFunction(param.params, &Sin<double>);
  } else if (function == "asin") {
    return TestUnaryFunction(param.params, &Asin<double>);
  } else if (function == "sinh") {
    return TestUnaryFunction(param.params, &Sinh<double>);
  } else if (function == "asinh") {
    return TestUnaryFunction(param.params, &Asinh<double>);
  } else if (function == "tan") {
    return TestUnaryFunction(param.params, &Tan<double>);
  } else if (function == "atan") {
    return TestUnaryFunction(param.params, &Atan<double>);
  } else if (function == "tanh") {
    return TestUnaryFunction(param.params, &Tanh<double>);
  } else if (function == "atanh") {
    return TestUnaryFunction(param.params, &Atanh<double>);
  } else if (function == "atan2") {
    return TestBinaryFunction(param.params, &Atan2<double>);
  } else if (function == "round") {
    switch (param.params.param(0).type_kind()) {
      case TYPE_FLOAT:
        if (param.params.num_params() == 1) {
          return TestUnaryFunction(param.params, &Round<float>);
        } else {
          return TestBinaryFunction(param.params, &RoundDecimal<float>);
        }
      case TYPE_DOUBLE:
        if (param.params.num_params() == 1) {
          return TestUnaryFunction(param.params, &Round<double>);
        } else {
          return TestBinaryFunction(param.params, &RoundDecimal<double>);
        }
      case TYPE_NUMERIC:
        if (param.params.num_params() == 1) {
          return TestUnaryFunction(param.params, &Round<NumericValue>);
        } else {
          return TestBinaryFunction(param.params, &RoundDecimal<NumericValue>);
        }
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "trunc") {
    switch (param.params.param(0).type_kind()) {
      case TYPE_FLOAT:
        if (param.params.num_params() == 1) {
          return TestUnaryFunction(param.params, &Trunc<float>);
        } else {
          return TestBinaryFunction(param.params, &TruncDecimal<float>);
        }
      case TYPE_DOUBLE:
        if (param.params.num_params() == 1) {
          return TestUnaryFunction(param.params, &Trunc<double>);
        } else {
          return TestBinaryFunction(param.params, &TruncDecimal<double>);
        }
      case TYPE_NUMERIC:
        if (param.params.num_params() == 1) {
          return TestUnaryFunction(param.params, &Trunc<NumericValue>);
        } else {
          return TestBinaryFunction(param.params, &TruncDecimal<NumericValue>);
        }
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "ceil" || function == "ceiling") {
    switch (param.params.param(0).type_kind()) {
      case TYPE_FLOAT:
        return TestUnaryFunction(param.params, &Ceil<float>);
      case TYPE_DOUBLE:
        return TestUnaryFunction(param.params, &Ceil<double>);
      case TYPE_NUMERIC:
        return TestUnaryFunction(param.params, &Ceil<NumericValue>);
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else if (function == "floor") {
    switch (param.params.param(0).type_kind()) {
      case TYPE_FLOAT:
        return TestUnaryFunction(param.params, &Floor<float>);
      case TYPE_DOUBLE:
        return TestUnaryFunction(param.params, &Floor<double>);
      case TYPE_NUMERIC:
        return TestUnaryFunction(param.params, &Floor<NumericValue>);
      default:
        FAIL() << "unrecognized type for " << function;
    }
  } else {
    FAIL() << "Unrecognized function: " << function;
  }
}

INSTANTIATE_TEST_SUITE_P(Math, MathTemplateTest,
                         testing::ValuesIn(GetFunctionTestsMath()));
INSTANTIATE_TEST_SUITE_P(Trigonometry, MathTemplateTest,
                         testing::ValuesIn(GetFunctionTestsTrigonometric()));
INSTANTIATE_TEST_SUITE_P(Rounding, MathTemplateTest,
                         testing::ValuesIn(GetFunctionTestsRounding()));

namespace {

TEST(NumericPowTest, ErrorMessage) {
  // POW is expected to produce a "floating point error" (rather than "floating
  // point overflow").
  NumericValue out;
  absl::Status status;
  EXPECT_FALSE(Pow<NumericValue>(NumericValue::MaxValue(),
                                 NumericValue::MaxValue(), &out, &status));
  EXPECT_THAT(
      status,
      ::zetasql_base::testing::StatusIs(
          absl::StatusCode::kOutOfRange,
          ::testing::HasSubstr("Floating point error in function: POW")));
}

}  // namespace

}  // namespace functions
}  // namespace zetasql
