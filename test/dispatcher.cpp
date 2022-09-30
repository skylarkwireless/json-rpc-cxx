#include "doctest/doctest.h"
#include <iostream>
#include <jsonrpccxx/dispatcher.hpp>

using namespace jsonrpccxx;
using namespace std;

static string procCache;
static unsigned int add_function(unsigned int a, unsigned int b) { return a + b; }
static unsigned int add_function2(unsigned int a, unsigned int b, unsigned int c) { return a + b + c; }
static void some_procedure(const string &param) { procCache = param; }

TEST_CASE("add and invoke positional") {
  Dispatcher d;
  CHECK(d.Add("some method", GetHandle(&add_function)));
  CHECK(!d.Add("some method", GetHandle(&add_function)));
  CHECK(d.InvokeMethod("some method", {11, 22}) == 33);

  procCache = "";
  CHECK(d.Add("some notification", GetHandle(&some_procedure)));
  CHECK(!d.Add("some notification", GetHandle(&some_procedure)));
  d.InvokeNotification("some notification", {"some string"});
  CHECK(procCache == "some string");
}

TEST_CASE("invoking supported named parameter") {
  Dispatcher d;
  CHECK(d.Add("some method", GetHandle(&add_function), {"a", "b"}));
  REQUIRE(d.InvokeMethod("some method", {{"a", 11}, {"b", 22}}) == 33);

  procCache = "";
  CHECK(d.Add("some notification", GetHandle(&some_procedure), {"param"}));
  json p = {{"param", "some string"}};
  d.InvokeNotification("some notification", p);
  CHECK(procCache == "some string");
}

TEST_CASE("invoking missing named parameter") {
  Dispatcher d;
  CHECK(d.Add("some method", GetHandle(&add_function), {"a", "b"}));
  REQUIRE_THROWS_WITH(d.InvokeMethod("some method", {{"a", 11}, {"xx", 22}}), "-32602: invalid parameter: missing named parameter \"b\"");

  procCache = "";
  CHECK(d.Add("some notification", GetHandle(&some_procedure), {"param"}));
  json p = {{"param2", "some string"}};
  REQUIRE_THROWS_WITH(d.InvokeNotification("some notification", p), "-32602: invalid parameter: missing named parameter \"param\"");
  CHECK(procCache.empty());
}

TEST_CASE("invoking wrong type namedparameter") {
  Dispatcher d;
  CHECK(d.Add("some method", GetHandle(&add_function), {"a", "b"}));
  REQUIRE_THROWS_WITH(d.InvokeMethod("some method", {{"a", "asdfasdf"}, {"b", -7}}), "-32602: invalid parameter: must be unsigned integer, but is string for parameter \"a\"");
  REQUIRE_THROWS_WITH(d.InvokeMethod("some method", {{"a", -10}, {"b", -7}}), "-32602: invalid parameter: must be unsigned integer, but is integer for parameter \"a\"");
}

TEST_CASE("error on invoking unsupported named parameter") {
  Dispatcher d;
  CHECK(d.Add("some method", GetHandle(&add_function)));
  REQUIRE_THROWS_WITH(d.InvokeMethod("some method", {{"a", 11}, {"b", 22}}), "-32602: invalid parameter: procedure doesn't support named parameter");

  CHECK(d.Add("some notification", GetHandle(&some_procedure)));
  json p = {{"param", "some string"}};
  REQUIRE_THROWS_WITH(d.InvokeNotification("some notification", p), "-32602: invalid parameter: procedure doesn't support named parameter");
}

TEST_CASE("passing invalid literal as param") {
    Dispatcher d;
    CHECK(d.Add("some method", GetHandle(&add_function)));
    REQUIRE_THROWS_WITH(d.InvokeMethod("some method", true), "-32600: invalid request: params field must be an array, object");
}

TEST_CASE("dispatching unknown procedures") {
    Dispatcher d;
    REQUIRE_THROWS_WITH(d.InvokeMethod("some method", {1}), "-32601: method not found: some method");
    REQUIRE_THROWS_WITH(d.InvokeNotification("some notification", {1}), "-32601: notification not found: some notification");
}

TEST_CASE("invalid param types") {
    Dispatcher d;
    CHECK(d.Add("some method", GetHandle(&add_function)));
    CHECK_THROWS_WITH(d.InvokeMethod("some method", {"string1", "string2"}), "-32602: invalid parameter: must be unsigned integer, but is string for parameter 0");
}

TEST_CASE("checking for method name") {
    Dispatcher d;
    CHECK(!d.ContainsMethod("some method"));
    CHECK(!d.Contains("some method"));
    CHECK(d.MethodNames().empty());

    CHECK(d.Add("some method", GetHandle(&add_function)));
    REQUIRE(d.ContainsMethod("some method"));
    REQUIRE(d.Contains("some method"));
    CHECK(d.InvokeMethod("some method", {11, 22}) == 33);
    const auto methodNames = d.MethodNames();
    CHECK(methodNames.size() == 1U);
    CHECK(methodNames[0] == "some method");

    CHECK(d.Remove("some method"));
    CHECK(!d.ContainsMethod("some method"));
    CHECK(!d.Contains("some method"));
    CHECK(d.MethodNames().empty());
}

TEST_CASE("checking for notification name") {
    Dispatcher d;
    CHECK(!d.ContainsNotification("some notification"));
    CHECK(!d.Contains("some notification"));
    CHECK(d.NotificationNames().empty());

    CHECK(d.Add("some notification", GetHandle(&some_procedure)));
    REQUIRE(d.ContainsNotification("some notification"));
    REQUIRE(d.Contains("some notification"));
    json p = {{"param", "some string"}};
    d.InvokeNotification("some notification", {"some string"});
    const auto notificationNames = d.NotificationNames();
    CHECK(notificationNames.size() == 1U);
    CHECK(notificationNames[0] == "some notification");

    CHECK(d.Remove("some notification"));
    CHECK(!d.ContainsNotification("some notification"));
    CHECK(!d.Contains("some notification"));
    CHECK(d.NotificationNames().empty());
}

TEST_CASE("checking adding calls without wrapping in a handle") {

  // Raw function pointer
  {
    Dispatcher d;

    CHECK(d.Add("add_function", "Add function", add_function, {"a", "b"}, {"A", "B"}));
    REQUIRE(d.ContainsMethod("add_function"));
    REQUIRE(d.Contains("add_function"));
    REQUIRE(d.InvokeMethod("add_function", {5, 10}) == 15);
    REQUIRE(d.MethodDocstring("add_function") == "Add function");

    const auto paramNames = d.MethodParamNames("add_function");
    REQUIRE(paramNames.size() == 2);
    CHECK(paramNames[0] == "a");
    CHECK(paramNames[1] == "b");

    const auto paramTypes = d.MethodParamTypes("add_function");
    REQUIRE(paramTypes.size() == 2);
    CHECK(paramTypes[0] == "unsigned integer");
    CHECK(paramTypes[1] == "unsigned integer");

    const auto paramDocstrings = d.MethodParamDocstrings("add_function");
    REQUIRE(paramDocstrings.size() == 2);
    CHECK(paramDocstrings[0] == "A");
    CHECK(paramDocstrings[1] == "B");

    // Check force adding with this function signature
    CHECK(not d.Add("add_function", "Add function", add_function2, ParamArgsMap{{"a", "A"}, {"b", "B"}, {"c", "C"}}));
    d.ForceAdd("add_function", "Add function", add_function2, ParamArgsMap{{"a", "A"}, {"b", "B"}, {"c", "C"}});
    REQUIRE(d.InvokeMethod("add_function", {5, 10, 15}) == 30);
  }

  // Lambda
  {
    Dispatcher d;

    auto mismatched_fma = [](int a, float b, unsigned c) -> double
    {
      return (double(a) * double(b)) + double(c);
    };
    auto mismatched_fma2 = [](double a, unsigned b, int c) -> float
    {
      return (float(a) * float(b)) + float(c);
    };

    CHECK(d.Add(
      "mismatched_fma",
      "Perform an FMA with different parameter types",
      mismatched_fma,
      ParamArgsMap{{"a", "A"}, {"b", "B"}, {"c", "C"}}));
    REQUIRE(d.ContainsMethod("mismatched_fma"));
    REQUIRE(d.Contains("mismatched_fma"));
    REQUIRE(d.InvokeMethod("mismatched_fma", {5, 10.0f, 20}) == 70.0);
    REQUIRE(d.MethodDocstring("mismatched_fma") == "Perform an FMA with different parameter types");

    const auto paramNames = d.MethodParamNames("mismatched_fma");
    REQUIRE(paramNames.size() == 3);
    CHECK(paramNames[0] == "a");
    CHECK(paramNames[1] == "b");
    CHECK(paramNames[2] == "c");

    auto paramTypes = d.MethodParamTypes("mismatched_fma");
    REQUIRE(paramTypes.size() == 3);
    CHECK(paramTypes[0] == "integer");
    CHECK(paramTypes[1] == "float");
    CHECK(paramTypes[2] == "unsigned integer");

    const auto paramDocstrings = d.MethodParamDocstrings("mismatched_fma");
    REQUIRE(paramDocstrings.size() == 3);
    CHECK(paramDocstrings[0] == "A");
    CHECK(paramDocstrings[1] == "B");
    CHECK(paramDocstrings[2] == "C");

    // Check force adding with this function signature
    CHECK(not d.Add("mismatched_fma", "Perform an FMA with different parameter types", mismatched_fma2, ParamArgsMap{{"a", "A"}, {"b", "B"}, {"c", "C"}}));
    d.ForceAdd("mismatched_fma", "Perform an FMA with different parameter types", mismatched_fma2, ParamArgsMap{{"a", "A"}, {"b", "B"}, {"c", "C"}});
    REQUIRE(d.ContainsMethod("mismatched_fma"));

    paramTypes = d.MethodParamTypes("mismatched_fma");
    REQUIRE(paramTypes.size() == 3);
    CHECK(paramTypes[0] == "float");
    CHECK(paramTypes[1] == "unsigned integer");
    CHECK(paramTypes[2] == "integer");
  }

  // Class function
  {
    class TestClass
    {
    public:
      int value{0};

      int addAndGetValue(int addAmount)
      {
        value += addAmount;
        return value;
      }

      int addAndGetValue2(int addAmount1, int addAmount2)
      {
        value += addAmount1;
        value += addAmount2;
        return value;
      }
    };

    Dispatcher d;
    TestClass cls;

    // So we don't have a default value.
    REQUIRE(cls.addAndGetValue(30) == 30);

    CHECK(d.Add("class_add_and_get_value", "Add to a class's field and return the new value", &TestClass::addAndGetValue, &cls, {"addAmount"}, {"Add amount"}));
    REQUIRE(d.ContainsMethod("class_add_and_get_value"));
    REQUIRE(d.Contains("class_add_and_get_value"));
    REQUIRE(d.InvokeMethod("class_add_and_get_value", {20}) == 50);
    REQUIRE(d.MethodDocstring("class_add_and_get_value") == "Add to a class's field and return the new value");

    const auto paramNames = d.MethodParamNames("class_add_and_get_value");
    REQUIRE(paramNames.size() == 1);
    CHECK(paramNames[0] == "addAmount");

    const auto paramTypes = d.MethodParamTypes("class_add_and_get_value");
    REQUIRE(paramTypes.size() == 1);
    CHECK(paramTypes[0] == "integer");

    const auto paramDocstrings = d.MethodParamDocstrings("class_add_and_get_value");
    REQUIRE(paramDocstrings.size() == 1);
    CHECK(paramDocstrings[0] == "Add amount");

    // Check force adding with this function signature
    CHECK(not d.Add(
        "class_add_and_get_value",
        "Add to a class's field and return the new value",
        &TestClass::addAndGetValue2,
        &cls,
        {"addAmount1", "addAmount2"},
        {"Add amount", "Add amount 2"}));
    d.ForceAdd(
      "class_add_and_get_value",
      "Add to a class's field and return the new value",
      &TestClass::addAndGetValue2,
      &cls,
      ParamArgsMap{{"addAmount1", "Add amount"}, {"addAmount2", "Add amount 2"}});
    REQUIRE(d.ContainsMethod("class_add_and_get_value"));
  }

  // std::function
  {
    std::function add_std_function(add_function);
    std::function add_std_function2(add_function2);

    Dispatcher d;

    CHECK(d.Add("add_std_function", "Add std::function", add_std_function, ParamArgsMap{{"a", "A"}, {"b", "B"}}));
    REQUIRE(d.ContainsMethod("add_std_function"));
    REQUIRE(d.Contains("add_std_function"));
    REQUIRE(d.InvokeMethod("add_std_function", {5, 10}) == 15);
    REQUIRE(d.MethodDocstring("add_std_function") == "Add std::function");

    const auto paramNames = d.MethodParamNames("add_std_function");
    REQUIRE(paramNames.size() == 2);
    CHECK(paramNames[0] == "a");
    CHECK(paramNames[1] == "b");

    const auto paramTypes = d.MethodParamTypes("add_std_function");
    REQUIRE(paramTypes.size() == 2);
    CHECK(paramTypes[0] == "unsigned integer");
    CHECK(paramTypes[1] == "unsigned integer");

    const auto paramDocstrings = d.MethodParamDocstrings("add_std_function");
    REQUIRE(paramDocstrings.size() == 2);
    CHECK(paramDocstrings[0] == "A");
    CHECK(paramDocstrings[1] == "B");

    // Check force adding with this function signature
    CHECK(not d.Add("add_std_function", "Add std::function", add_std_function2, {"a", "b", "c"}, {"A", "B", "C"}));
    d.Add("add_std_function", "Add std::function", add_std_function2, {"a", "b", "c"}, {"A", "B", "C"});
    REQUIRE(d.ContainsMethod("add_std_function"));
  }
}

TEST_CASE("checking mismatched number params when adding calls without wrapping in a handle") {
  Dispatcher d;

  // Not enough parameters
  {
    const auto expectedMsg = R"(Error registering RPC method "add_function": number of listed parameters (1) does not match registered method's parameter list (2).)";

    CHECK_THROWS_WITH(d.Add("add_function", "Add function", add_function, {"a"}, {"A"}), expectedMsg);
    CHECK(not d.ContainsMethod("add_function"));
  }

  // Too many parameters
  {
    const auto expectedMsg = R"(Error registering RPC method "add_function": number of listed parameters (3) does not match registered method's parameter list (2).)";

    CHECK_THROWS_WITH(d.Add("add_function", "Add function", add_function, {"a", "b", "c"}, {"A", "B", "C"}), expectedMsg);
    CHECK(not d.ContainsMethod("add_function"));
  }
}

TEST_CASE("checking adding two-parameter method without a handle and without arg docstrings") {
  Dispatcher d;

  class TestClass
  {
  public:
    int add(int lhs, int rhs)
    {
      return lhs + rhs;
    }
  };

  CHECK(d.Add("add_function", "Add function", add_function, {"a", "b"}));
  CHECK(d.InvokeMethod("add_function", {1, 2}) == 3);

  TestClass cls;
  CHECK(d.Add("class_add", "Class Add", &TestClass::add, &cls, {"lhs", "rhs"}));
  CHECK(d.InvokeMethod("class_add", {1, 2}) == 3);
}

TEST_CASE("checking adding const class method without a handle") {
  Dispatcher d;

  class TestClass
  {
  public:
    int add(int lhs, int rhs) const
    {
      return lhs + rhs;
    }
  };

  CHECK(d.Add("add_function", "Add function", add_function, {"a", "b"}));
  CHECK(d.InvokeMethod("add_function", {1, 2}) == 3);

  TestClass cls;
  CHECK(d.Add("class_add", "Class Add", &TestClass::add, &cls, {"lhs", "rhs"}));
  CHECK(d.InvokeMethod("class_add", {1, 2}) == 3);
}

TEST_CASE("checking parameter order is preserved with map-like initializer lists") {
  Dispatcher d;

  auto fcn = [](float z, unsigned y, int x)
  {
    return double(z + y + x);
  };

  CHECK(d.Add("fcn", "Test function", fcn, ParamArgsMap{{"z", "Z"}, {"y", "Y"}, {"x", "X"}}));

  const auto paramNames = d.MethodParamNames("fcn");
  CHECK(paramNames[0] == "z");
  CHECK(paramNames[1] == "y");
  CHECK(paramNames[2] == "x");

  const auto paramTypes = d.MethodParamTypes("fcn");
  CHECK(paramTypes[0] == "float");
  CHECK(paramTypes[1] == "unsigned integer");
  CHECK(paramTypes[2] == "integer");

  const auto paramDocstrings = d.MethodParamDocstrings("fcn");
  CHECK(paramDocstrings[0] == "Z");
  CHECK(paramDocstrings[1] == "Y");
  CHECK(paramDocstrings[2] == "X");
}
