#include "doctest/doctest.h"
#include "testserverconnector.hpp"
#include <iostream>
#include <vector>
#include <jsonrpccxx/server.hpp>

using namespace jsonrpccxx;
using namespace std;

static unsigned int add_function(unsigned int a, unsigned int b) { return a + b; }

struct Server2 {
  JsonRpc2Server server;
  TestServerConnector connector;

  Server2() : server(), connector(server) {}
};

TEST_CASE_FIXTURE(Server2, "v2_method_not_found") {
  connector.CallMethod(1, "some_invalid_method", nullptr);
  connector.VerifyMethodError(-32601, "method not found: some_invalid_method", 1);
}

TEST_CASE_FIXTURE(Server2, "v2_malformed_requests") {
  string name = "some_method";
  json params = nullptr;

  connector.SendRawRequest("dfasdf");
  connector.VerifyMethodError(-32700, "parse error", nullptr);
  connector.SendRawRequest("true");
  connector.VerifyMethodError(-32600, "invalid request: expected array or object", nullptr);

  connector.SendRequest({{"id", true}, {"method", name}, {"params", params}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32600, "invalid request: id field must be a number, string or null", nullptr);
  connector.SendRequest({{"id", {3}}, {"method", name}, {"params", params}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32600, "invalid request: id field must be a number, string or null", nullptr);
  connector.SendRequest({{"id", {{"a", "b"}}}, {"method", name}, {"params", params}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32600, "invalid request: id field must be a number, string or null", nullptr);
  connector.SendRequest({{"id", nullptr}, {"method", name}, {"params", params}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32601, "method not found: some_method", nullptr);

  connector.SendRequest({{"id", 1}, {"params", params}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32600, "invalid request: method field must be a string", 1);
  connector.SendRequest({{"id", 1}, {"method", 33}, {"params", params}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32600, "invalid request: method field must be a string", 1);
  connector.SendRequest({{"id", 1}, {"method", true}, {"params", params}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32600, "invalid request: method field must be a string", 1);

  connector.SendRequest({{"id", 1}, {"method", name}, {"params", params}, {"jsonrpc", "3.0"}});
  connector.VerifyMethodError(-32600, R"(invalid request: missing jsonrpc field set to "2.0")", 1);
  connector.SendRequest({{"id", 1}, {"method", name}, {"params", params}, {"jsonrpc", nullptr}});
  connector.VerifyMethodError(-32600, R"(invalid request: missing jsonrpc field set to "2.0")", 1);
  connector.SendRequest({{"id", 1}, {"method", name}, {"params", params}});
  connector.VerifyMethodError(-32600, R"(invalid request: missing jsonrpc field set to "2.0")", 1);

  connector.SendRequest({{"id", 1}, {"method", name}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32601, "method not found: some_method", 1);
  connector.SendRequest({{"id", 1}, {"method", name}, {"params", true}, {"jsonrpc", "2.0"}});
  connector.VerifyMethodError(-32600, "invalid request: params field must be an array, object or null", 1);
}

enum class category { ord, cc };

NLOHMANN_JSON_SERIALIZE_ENUM(category, {{category::ord, "order"}, {category::cc, "cc"}})

struct product {
  product() : id(), price(), name(), cat() {}
  int id;
  double price;
  string name;
  category cat;
};

void to_json(json &j, const product &p);
void from_json(const json &j, product &p);

class TestServer {
public:
  TestServer() : param_proc(), param_a(), param_b(), catalog() {}

  unsigned int add_function(unsigned int a, unsigned int b) {
    this->param_a = a;
    this->param_b = b;
    return a + b;
  }
  unsigned int div_function(unsigned int a, unsigned int b) {
    this->param_a = a;
    this->param_b = b;
    if (b != 0)
      return a / b;
    else
      throw JsonRpcException(-32602, "b must not be 0");
  }
  void some_procedure(const string &param) { param_proc = param; }
  bool add_products(const vector<product> &products) {
    std::copy(products.begin(), products.end(), std::back_inserter(catalog));
    return true;
  };

  void dirty_notification() { throw std::exception(); }
  int dirty_method(int a, int b) { to_string(a+b); throw std::exception(); }
  int dirty_method2(int a, int b) { throw (a+b); }

  string param_proc;
  int param_a;
  int param_b;
  vector<product> catalog;
};

TEST_CASE_FIXTURE(Server2, "v2_invocations") {
  TestServer t;
  REQUIRE(server.Add("add_function", GetHandle(&TestServer::add_function, t), {"a", "b"}));
  REQUIRE(server.Add("div_function", GetHandle(&TestServer::div_function, t), {"a", "b"}));
  REQUIRE(server.Add("some_procedure", GetHandle(&TestServer::some_procedure, t), {"param"}));
  REQUIRE(server.Add("add_products", GetHandle(&TestServer::add_products, t), {"products"}));
  REQUIRE(server.Add("dirty_notification", GetHandle(&TestServer::dirty_notification, t), {"products"}));
  REQUIRE(server.Add("dirty_method", GetHandle(&TestServer::dirty_method, t), {"a", "b"}));
  REQUIRE(server.Add("dirty_method2", GetHandle(&TestServer::dirty_method2, t), {"a", "b"}));

  REQUIRE(!server.Add("dirty_method2", GetHandle(&TestServer::dirty_method2, t), {"a", "b"}));
  REQUIRE(!server.Add("rpc.something", GetHandle(&TestServer::dirty_method2, t), {"a", "b"}));
  REQUIRE(!server.Add("rpc.", GetHandle(&TestServer::dirty_method2, t), {"a", "b"}));
  REQUIRE(!server.Add("rpc.somenotification", GetHandle(&TestServer::dirty_notification, t), {"a", "b"}));
  REQUIRE(server.Add("rpc", GetHandle(&TestServer::dirty_method2, t), {"a", "b"}));

  connector.CallMethod(1, "add_function", {{"a", 3}, {"b", 4}});
  CHECK(connector.VerifyMethodResult(1) == 7);
  CHECK(t.param_a == 3);
  CHECK(t.param_b == 4);

  connector.CallNotification("some_procedure", {{"param", "something set"}});
  connector.VerifyNotificationResult();
  CHECK(t.param_proc == "something set");

  json params = json::parse(
      R"({"products": [{"id": 1, "price": 23.3, "name": "some product", "category": "cc"},{"id": 2, "price": 23.4, "name": "some product 2", "category": "order"}]})");

  connector.CallMethod(1, "add_products", params);
  CHECK(connector.VerifyMethodResult(1) == true);
  CHECK(t.catalog.size() == 2);
  CHECK(t.catalog[0].id == 1);
  CHECK(t.catalog[0].name == "some product");
  CHECK(t.catalog[0].price == 23.3);
  CHECK(t.catalog[0].cat == category::cc);
  CHECK(t.catalog[1].id == 2);
  CHECK(t.catalog[1].name == "some product 2");
  CHECK(t.catalog[1].price == 23.4);
  CHECK(t.catalog[1].cat == category::ord);

  connector.CallNotification("dirty_notification", nullptr);
  connector.VerifyNotificationResult();
  connector.CallMethod(1, "dirty_method", {{"a", 3}, {"b", 0}});
  connector.VerifyMethodError(-32603, "internal server error", 1);
  connector.CallMethod(1, "div_function", {{"a", 3}, {"b", 0}});
  connector.VerifyMethodError(-32602, "b must not be 0", 1);
  connector.CallMethod(1, "div_function", {{"a", 6}, {"b", 2}});
  CHECK(connector.VerifyMethodResult(1) == 3);
  connector.CallMethod(1, "dirty_method2", {{"a", 3}, {"b", 0}});
  connector.VerifyMethodError(-32603, "internal server error", 1);
}

TEST_CASE_FIXTURE(Server2, "v2_batch") {
  TestServer t;
  REQUIRE(server.Add("add_function", GetHandle(&TestServer::add_function, t), {"a", "b"}));

  json batchcall;

  batchcall.push_back(connector.BuildMethodCall(1, "add_function", {{"a", 3}, {"b", 4}}));
  batchcall.push_back(connector.BuildMethodCall(2, "add_function", {{"a", 300}, {"b", 4}}));
  batchcall.push_back(connector.BuildMethodCall(3, "add_function", {{"a", 300}}));
  batchcall.push_back("");

  connector.SendRequest(batchcall);
  json batchresponse = connector.VerifyBatchResponse();
  REQUIRE(batchresponse.size() == 4);

  REQUIRE(TestServerConnector::VerifyMethodResult(1, batchresponse[0]) == 7);
  REQUIRE(TestServerConnector::VerifyMethodResult(2, batchresponse[1]) == 304);
  TestServerConnector::VerifyMethodError(-32602, R"(missing named parameter "b")", 3, batchresponse[2]);
  TestServerConnector::VerifyMethodError(-32600, R"(invalid request)", nullptr, batchresponse[3]);

  connector.SendRawRequest("[]");
  batchresponse = connector.VerifyBatchResponse();
  REQUIRE(batchresponse.empty());
}

TEST_CASE_FIXTURE(Server2, "v2_check_functions") {
  TestServer t;
  CHECK(server.MethodNames().empty());
  CHECK(server.NotificationNames().empty());

  REQUIRE(server.Add("add_function", GetHandle(&TestServer::add_function, t), {"a", "b"}));
  REQUIRE(server.Add("div_function", GetHandle(&TestServer::div_function, t), {"a", "b"}));
  REQUIRE(server.Add("some_procedure", GetHandle(&TestServer::some_procedure, t), {"param"}));
  REQUIRE(server.Add("add_products", GetHandle(&TestServer::add_products, t), {"products"}));
  REQUIRE(server.Add("dirty_notification", GetHandle(&TestServer::dirty_notification, t), {"products"}));
  REQUIRE(server.Add("dirty_method", GetHandle(&TestServer::dirty_method, t), {"a", "b"}));
  REQUIRE(server.Add("dirty_method2", GetHandle(&TestServer::dirty_method2, t), {"a", "b"}));
  REQUIRE(server.Add("rpc", GetHandle(&TestServer::dirty_method2, t), {"a", "b"}));

  CHECK(server.ContainsMethod("add_function"));
  CHECK(server.ContainsMethod("div_function"));
  CHECK(server.ContainsMethod("add_products"));
  CHECK(server.ContainsMethod("dirty_method"));
  CHECK(server.ContainsMethod("dirty_method2"));
  CHECK(server.ContainsMethod("rpc"));
  CHECK(server.ContainsNotification("some_procedure"));
  CHECK(server.ContainsNotification("dirty_notification"));

  CHECK(server.Contains("add_function"));
  CHECK(server.Contains("div_function"));
  CHECK(server.Contains("add_products"));
  CHECK(server.Contains("dirty_method"));
  CHECK(server.Contains("dirty_method2"));
  CHECK(server.Contains("rpc"));
  CHECK(server.Contains("some_procedure"));
  CHECK(server.Contains("dirty_notification"));

  const auto methodNames = server.MethodNames();
  CHECK(methodNames.size() == 6U);
  CHECK(std::find(methodNames.begin(), methodNames.end(), "add_function") != methodNames.end());
  CHECK(std::find(methodNames.begin(), methodNames.end(), "div_function") != methodNames.end());
  CHECK(std::find(methodNames.begin(), methodNames.end(), "add_products") != methodNames.end());
  CHECK(std::find(methodNames.begin(), methodNames.end(), "dirty_method") != methodNames.end());
  CHECK(std::find(methodNames.begin(), methodNames.end(), "dirty_method2") != methodNames.end());
  CHECK(std::find(methodNames.begin(), methodNames.end(), "rpc") != methodNames.end());

  const auto notificationNames = server.NotificationNames();
  CHECK(notificationNames.size() == 2U);
  CHECK(std::find(notificationNames.begin(), notificationNames.end(), "some_procedure") != notificationNames.end());
  CHECK(std::find(notificationNames.begin(), notificationNames.end(), "dirty_notification") != notificationNames.end());

  REQUIRE(server.Remove("add_function"));
  REQUIRE(server.Remove("div_function"));
  REQUIRE(server.Remove("add_products"));
  REQUIRE(server.Remove("dirty_method"));
  REQUIRE(server.Remove("dirty_method2"));
  REQUIRE(server.Remove("rpc"));
  REQUIRE(server.Remove("some_procedure"));
  REQUIRE(server.Remove("dirty_notification"));
  CHECK(server.MethodNames().empty());
  CHECK(server.NotificationNames().empty());
}

TEST_CASE("checking adding calls without wrapping in a handle") {

  // Raw function pointer
  {
    JsonRpc2Server server;

    CHECK(server.Add(
        "add_function",
        "Add function",
        add_function,
        ParamArgsMap{{"a", "A"}, {"b", "B"}}));
    REQUIRE(server.ContainsMethod("add_function"));
    REQUIRE(server.Contains("add_function"));
    REQUIRE(server.MethodDocstring("add_function") == "Add function");

    const auto paramNames = server.MethodParamNames("add_function");
    REQUIRE(paramNames.size() == 2);
    CHECK(paramNames[0] == "a");
    CHECK(paramNames[1] == "b");

    const auto paramTypes = server.MethodParamTypes("add_function");
    REQUIRE(paramTypes.size() == 2);
    CHECK(paramTypes[0] == "unsigned integer");
    CHECK(paramTypes[1] == "unsigned integer");

    const auto paramDocstrings = server.MethodParamDocstrings("add_function");
    REQUIRE(paramDocstrings.size() == 2);
    CHECK(paramDocstrings[0] == "A");
    CHECK(paramDocstrings[1] == "B");
  }

  // Lambda
  {
    JsonRpc2Server server;

    auto mismatched_fma = [](int a, float b, unsigned c) -> double
    {
      return (double(a) * double(b)) + double(c);
    };

    CHECK(server.Add("mismatched_fma", "Perform an FMA with different parameter types", mismatched_fma, {"a", "b", "c"}, {"A", "B", "C"}));
    REQUIRE(server.ContainsMethod("mismatched_fma"));
    REQUIRE(server.Contains("mismatched_fma"));
    REQUIRE(server.MethodDocstring("mismatched_fma") == "Perform an FMA with different parameter types");

    const auto paramNames = server.MethodParamNames("mismatched_fma");
    REQUIRE(paramNames.size() == 3);
    CHECK(paramNames[0] == "a");
    CHECK(paramNames[1] == "b");
    CHECK(paramNames[2] == "c");

    const auto paramTypes = server.MethodParamTypes("mismatched_fma");
    REQUIRE(paramTypes.size() == 3);
    CHECK(paramTypes[0] == "integer");
    CHECK(paramTypes[1] == "float");
    CHECK(paramTypes[2] == "unsigned integer");

    const auto paramDocstrings = server.MethodParamDocstrings("mismatched_fma");
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

    JsonRpc2Server server;
    TestClass cls;

    CHECK(server.Add(
      "class_add_and_get_value",
      "Add to a class's field and return the new value",
      &TestClass::addAndGetValue,
      &cls,
      ParamArgsMap{{"addAmount", "Add amount"}}));
    REQUIRE(server.ContainsMethod("class_add_and_get_value"));
    REQUIRE(server.Contains("class_add_and_get_value"));
    REQUIRE(server.MethodDocstring("class_add_and_get_value") == "Add to a class's field and return the new value");

    const auto paramNames = server.MethodParamNames("class_add_and_get_value");
    REQUIRE(paramNames.size() == 1);
    CHECK(paramNames[0] == "addAmount");

    const auto paramTypes = server.MethodParamTypes("class_add_and_get_value");
    REQUIRE(paramTypes.size() == 1);
    CHECK(paramTypes[0] == "integer");

    const auto paramDocstrings = server.MethodParamDocstrings("class_add_and_get_value");
    REQUIRE(paramDocstrings.size() == 1);
    CHECK(paramDocstrings[0] == "Add amount");
  }

  // std::function
  {
    std::function add_std_function(add_function);

    JsonRpc2Server server;

    CHECK(server.Add("add_std_function", "Add std::function", add_std_function, {"a", "b"}, {"A", "B"}));
    REQUIRE(server.ContainsMethod("add_std_function"));
    REQUIRE(server.Contains("add_std_function"));
    REQUIRE(server.MethodDocstring("add_std_function") == "Add std::function");

    const auto paramNames = server.MethodParamNames("add_std_function");
    REQUIRE(paramNames.size() == 2);
    CHECK(paramNames[0] == "a");
    CHECK(paramNames[1] == "b");

    const auto paramTypes = server.MethodParamTypes("add_std_function");
    REQUIRE(paramTypes.size() == 2);
    CHECK(paramTypes[0] == "unsigned integer");
    CHECK(paramTypes[1] == "unsigned integer");

    const auto paramDocstrings = server.MethodParamDocstrings("add_std_function");
    REQUIRE(paramDocstrings.size() == 2);
    CHECK(paramDocstrings[0] == "A");
    CHECK(paramDocstrings[1] == "B");
  }
}

TEST_CASE("checking mismatched number params when adding calls without wrapping in a handle") {
  JsonRpc2Server server;

  // Not enough parameters
  {
    const auto expectedMsg = R"(Error registering RPC method "add_function": number of listed parameters (1) does not match registered method's parameter list (2).)";

    CHECK_THROWS_WITH(server.Add("add_function", "Add function", add_function, {"a"}, {"A"}), expectedMsg);
    CHECK(not server.ContainsMethod("add_function"));
  }

  // Too many parameters
  {
    const auto expectedMsg = R"(Error registering RPC method "add_function": number of listed parameters (3) does not match registered method's parameter list (2).)";

    CHECK_THROWS_WITH(server.Add("add_function", "Add function", add_function, {"a", "b", "c"}, {"A", "B", "C"}), expectedMsg);
    CHECK(not server.ContainsMethod("add_function"));
  }
}

TEST_CASE("checking adding two-parameter method without a handle and without arg docstrings") {
  JsonRpc2Server server;

  class TestClass
  {
  public:
    int add(int lhs, int rhs)
    {
      return lhs + rhs;
    }
  };

  CHECK(server.Add("add_function", "Add function", add_function, {"a", "b"}));

  TestClass cls;
  CHECK(server.Add("class_add", "Class Add", &TestClass::add, &cls, {"lhs", "rhs"}));
}
