#pragma once
#include "nlohmann/json.hpp"
#include <exception>
#include <string>

namespace jsonrpccxx {
  typedef nlohmann::json json;

  static inline bool has_key(const json &v, const std::string &key) { return v.find(key) != v.end(); }
  static inline bool has_key_type(const json &v, const std::string &key, json::value_t type) { return has_key(v, key) && v.at(key).type() == type; }
  static inline bool valid_id(const json &request) {
    return has_key(request, "id") && (request["id"].is_number() || request["id"].is_string() || request["id"].is_null());
  }
  static inline bool valid_id_not_null(const json &request) { return has_key(request, "id") && (request["id"].is_number() || request["id"].is_string()); }

  class JsonRpcException : public std::exception {
  public:
    JsonRpcException(int code, const std::string &message) noexcept : code(code), message(message), data(nullptr), err(std::to_string(code) + ": " + message) {}
    JsonRpcException(int code, const std::string &message, const json &data) noexcept
        : code(code), message(message), data(data), err(std::to_string(code) + ": " + message + ", data: " + data.dump()) {}

    int Code() const { return code; }
    const std::string &Message() const { return message; }
    const json &Data() const { return data; }

    const char *what() const noexcept override { return err.c_str(); }

    static inline JsonRpcException fromJson(const json &value) {
      bool has_code = has_key_type(value, "code", json::value_t::number_integer);
      bool has_message = has_key_type(value, "message", json::value_t::string);
      bool has_data = has_key(value, "data");
      if (has_code && has_message) {
        if (has_data) {
          return JsonRpcException(value["code"], value["message"], value["data"].get<json>());
        } else {
          return JsonRpcException(value["code"], value["message"]);
        }
      }
      return JsonRpcException(-32603, R"(invalid error response: "code" (negative number) and "message" (string) are required)");
    }

  private:
    int code;
    std::string message;
    json data;
    std::string err;
  };
} // namespace jsonrpccxx
