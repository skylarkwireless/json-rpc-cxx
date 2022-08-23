#include "doctest/doctest.h"
#include <iostream>
#include <jsonrpccxx/dispatcher.hpp>

using namespace jsonrpccxx;
using namespace std;

static string procCache;
static unsigned int add_function(unsigned int a, unsigned int b) { return a + b; }
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
  }

  // Lambda
  {
    Dispatcher d;

    auto mismatched_fma = [](int a, float b, unsigned c) -> double
    {
      return (double(a) * double(b)) + double(c);
    };

    CHECK(d.Add("mismatched_fma", "Perform an FMA with different parameter types", mismatched_fma, {"a", "b", "c"}, {"A", "B", "C"}));
    REQUIRE(d.ContainsMethod("mismatched_fma"));
    REQUIRE(d.Contains("mismatched_fma"));
    REQUIRE(d.InvokeMethod("mismatched_fma", {5, 10.0f, 20}) == 70.0);
    REQUIRE(d.MethodDocstring("mismatched_fma") == "Perform an FMA with different parameter types");

    const auto paramNames = d.MethodParamNames("mismatched_fma");
    REQUIRE(paramNames.size() == 3);
    CHECK(paramNames[0] == "a");
    CHECK(paramNames[1] == "b");
    CHECK(paramNames[2] == "c");

    const auto paramTypes = d.MethodParamTypes("mismatched_fma");
    REQUIRE(paramTypes.size() == 3);
    CHECK(paramTypes[0] == "integer");
    CHECK(paramTypes[1] == "float");
    CHECK(paramTypes[2] == "unsigned integer");

    const auto paramDocstrings = d.MethodParamDocstrings("mismatched_fma");
    REQUIRE(paramDocstrings.size() == 3);
    CHECK(paramDocstrings[0] == "A");
    CHECK(paramDocstrings[1] == "B");
    CHECK(paramDocstrings[2] == "C");
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
  }

  // std::function
  {
    std::function add_std_function(add_function);

    Dispatcher d;

    CHECK(d.Add("add_std_function", "Add std::function", add_std_function, {"a", "b"}, {"A", "B"}));
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
  }
}

// TODO: avoid signed, unsigned bool invocations
