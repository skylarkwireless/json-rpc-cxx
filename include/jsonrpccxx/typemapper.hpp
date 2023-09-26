#pragma once

#include "common.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

namespace jsonrpccxx {
  typedef std::function<json(const json &)> MethodHandle;
  typedef std::function<void(const json &)> NotificationHandle;

  typedef std::vector<std::string> NamedParamMapping;
  static NamedParamMapping NAMED_PARAM_MAPPING;

  // No, this isn't really a map, but this allows us to use map-like
  // initializer lists while maintaining the order we pass in parameters.
  typedef std::vector<std::pair<std::string, std::string>> ParamArgsMap;

  // Workaround due to forbidden partial template function specialisation
  template <typename T>
  struct type {};

  template <typename T, typename Allocator>
  constexpr json::value_t GetType(type<std::vector<T, Allocator>>) {
    return json::value_t::array;
  }
  template <typename T, size_t N>
  constexpr json::value_t GetType(type<std::array<T, N>>) {
    return json::value_t::array;
  }
  template <typename T, class Compare, class Allocator>
  constexpr json::value_t GetType(type<std::set<T,Compare,Allocator>>) {
    return json::value_t::array;
  }

  template <typename T>
  constexpr json::value_t GetType(type<T>) {
    if (std::is_enum<T>::value) {
      return json::value_t::string;
    }
    return json::value_t::object;
  }
  constexpr json::value_t GetType(type<void>) { return json::value_t::null; }
  constexpr json::value_t GetType(type<std::string>) { return json::value_t::string; }
  constexpr json::value_t GetType(type<bool>) { return json::value_t::boolean; }
  constexpr json::value_t GetType(type<float>) { return json::value_t::number_float; }
  constexpr json::value_t GetType(type<double>) { return json::value_t::number_float; }
  constexpr json::value_t GetType(type<long double>) { return json::value_t::number_float; }
  constexpr json::value_t GetType(type<short>) { return json::value_t::number_integer; }
  constexpr json::value_t GetType(type<int>) { return json::value_t::number_integer; }
  constexpr json::value_t GetType(type<long>) { return json::value_t::number_integer; }
  constexpr json::value_t GetType(type<long long>) { return json::value_t::number_integer; }
  constexpr json::value_t GetType(type<unsigned short>) { return json::value_t::number_unsigned; }
  constexpr json::value_t GetType(type<unsigned int>) { return json::value_t::number_unsigned; }
  constexpr json::value_t GetType(type<unsigned long>) { return json::value_t::number_unsigned; }
  constexpr json::value_t GetType(type<unsigned long long>) { return json::value_t::number_unsigned; }
  constexpr json::value_t GetType(type<std::filesystem::path>) { return json::value_t::string; }

  inline std::string type_name(json::value_t t) {
    switch (t) {
    case json::value_t::number_integer:
      return "integer";
    case json::value_t::boolean:
      return "boolean";
    case json::value_t::number_float:
      return "float";
    case json::value_t::number_unsigned:
      return "unsigned integer";
    case json::value_t::object:
      return "object";
    case json::value_t::array:
      return "array";
    case json::value_t::string:
      return "string";
    default:
      return "null";
    }
  }

  inline std::string make_error_prefix(const std::string &methodName, const std::string &paramName) {
    return methodName + ": invalid parameter \"" + paramName + "\"";
  }

  inline std::string make_error_message(const std::string &methodName, const std::string &paramName, const std::string &message) {
    return make_error_prefix(methodName, paramName) + " (" + message + ")";
  }

  inline std::string make_invalid_type_error_message(const std::string &methodName, const std::string &paramName, const json &x, json::value_t expectedType) {
    std::ostringstream errMsgStream;
    errMsgStream << make_error_prefix(methodName, paramName) << " (must be " << type_name(expectedType)
                 << ", but is " << type_name(x.type()) << ": " << x.dump() << ")";
    return errMsgStream.str();
  }

  template <typename T>
  inline std::string make_numeric_bounds_error_message(const std::string &methodName, const std::string &paramName, const json &x, json::value_t expectedType) {
    std::ostringstream errMsgStream;
    errMsgStream << make_error_prefix(methodName, paramName) << " (exceeds value range of " << type_name(expectedType)
                 << ": " << x.dump() << ")";
    return errMsgStream.str();
  }

  template <typename T>
  inline void check_param_type(const std::string &methodName, const std::string &paramName, const json &x, json::value_t expectedType, typename std::enable_if<std::is_arithmetic<T>::value>::type * = 0) {
    if (expectedType == json::value_t::number_unsigned && x.type() == json::value_t::number_integer) {
      if (x.get<long long int>() < 0)
        throw JsonRpcException(invalid_params, make_invalid_type_error_message(methodName, paramName, x, expectedType));
    } else if (x.type() == json::value_t::number_unsigned && expectedType == json::value_t::number_integer) {
      if (x.get<long long unsigned>() > (long long unsigned)std::numeric_limits<T>::max()) {
        throw JsonRpcException(invalid_params, make_numeric_bounds_error_message<T>(methodName, paramName, x, expectedType));
      }
    }
    else if ((x.type() == json::value_t::number_unsigned || x.type() == json::value_t::number_integer) && expectedType == json::value_t::number_float) {
      if (static_cast<long long int>(x.get<double>()) != x.get<long long int>()) {
        throw JsonRpcException(invalid_params, make_numeric_bounds_error_message<T>(methodName, paramName, x, expectedType));
      }
    } else if (x.type() != expectedType) {
      throw JsonRpcException(invalid_params, make_invalid_type_error_message(methodName, paramName, x, expectedType));
    }
  }

  template <typename T>
  inline void check_param_type(const std::string &methodName, const std::string &paramName, const json &x, json::value_t expectedType, typename std::enable_if<!std::is_arithmetic<T>::value>::type * = 0) {
    if (x.type() != expectedType) {
      throw JsonRpcException(invalid_params, make_invalid_type_error_message(methodName, paramName, x, expectedType));
    }
  }

  template <typename ReturnType, typename... ParamTypes, std::size_t... index>
  MethodHandle createMethodHandle(const std::string &methodName, const NamedParamMapping &paramNames, std::function<ReturnType(ParamTypes...)> method, std::index_sequence<index...> seq, NamedParamMapping &types) {
    (void)seq;

    (types.emplace_back(type_name(GetType(type<std::decay_t<ParamTypes>>()))), ...);

    if (paramNames.size() != sizeof...(ParamTypes)) {
      std::ostringstream errMsgStream;
      errMsgStream << "Error registering RPC method \"" << methodName << "\": number of parameter names ("
                   << paramNames.size() << ") does not match registered method's parameter list ("
                   << sizeof...(ParamTypes) << ").";

      throw std::invalid_argument(errMsgStream.str());
    }

    MethodHandle handle = [method, methodName, paramNames](const json &params) -> json {
      size_t actualSize = params.size();
      size_t formalSize = sizeof...(ParamTypes);
      // TODO: add lenient mode for backwards compatible additional params
      if (actualSize != formalSize) {
        throw JsonRpcException(invalid_params, methodName + ": invalid parameters (expected " + std::to_string(formalSize) + " argument(s), but found " + std::to_string(actualSize) + ")");
      }
      (check_param_type<typename std::decay<ParamTypes>::type>(methodName, paramNames[index], params[index], GetType(type<typename std::decay<ParamTypes>::type>())), ...);

      return method(params[index].get<typename std::decay<ParamTypes>::type>()...);
    };
    return handle;
  }

  template <typename ReturnType, typename... ParamTypes, std::size_t... index>
  inline MethodHandle createMethodHandle(const std::string &methodName, const NamedParamMapping &paramNames, std::function<ReturnType(ParamTypes...)> method, std::index_sequence<index...> seq) {
    NamedParamMapping _;
    return createMethodHandle(methodName, paramNames, method, seq, _);
  }

  template <typename ReturnType, typename... ParamTypes>
  MethodHandle methodHandle(const std::string &methodName, const NamedParamMapping &paramNames, std::function<ReturnType(ParamTypes...)> method) {
    return createMethodHandle(methodName, paramNames, method, std::index_sequence_for<ParamTypes...>{});
  }

  template <typename ReturnType, typename... ParamTypes>
  MethodHandle GetHandle(const std::string &methodName, const NamedParamMapping &paramNames, std::function<ReturnType(ParamTypes...)> f) {
    return methodHandle(methodName, paramNames, f);
  }
  // Mapping for c-style function pointers
  template <typename ReturnType, typename... ParamTypes>
  MethodHandle GetHandle(const std::string &methodName, const NamedParamMapping &paramNames, ReturnType (*f)(ParamTypes...)) {
    return GetHandle(methodName, paramNames, std::function<ReturnType(ParamTypes...)>(f));
  }

  inline MethodHandle GetUncheckedHandle(std::function<json(const json&)> f) {
    MethodHandle handle = [f](const json &params) -> json {
      return f(params);
    };
    return handle;
  }

  //
  // Notification mapping
  //
  template <typename... ParamTypes, std::size_t... index>
  NotificationHandle createNotificationHandle(const std::string &methodName, const NamedParamMapping &paramNames, std::function<void(ParamTypes...)> method, std::index_sequence<index...>) {
    NotificationHandle handle = [method, methodName, paramNames](const json &params) -> void {
      size_t actualSize = params.size();
      size_t formalSize = sizeof...(ParamTypes);
      // TODO: add lenient mode for backwards compatible additional params
      // if ((!allow_unkown_params && actualSize != formalSize) || (allow_unkown_params && actualSize < formalSize)) {
      if (actualSize != formalSize) {
        throw JsonRpcException(invalid_params, methodName + ": invalid parameters (expected " + std::to_string(formalSize) + " argument(s), but found " + std::to_string(actualSize) + ")");
      }
      (check_param_type<typename std::decay<ParamTypes>::type>(methodName, paramNames[index], params[index], GetType(type<typename std::decay<ParamTypes>::type>())), ...);
      method(params[index].get<typename std::decay<ParamTypes>::type>()...);
    };
    return handle;
  }

  template <typename... ParamTypes>
  NotificationHandle notificationHandle(const std::string &methodName, const NamedParamMapping &paramNames, std::function<void(ParamTypes...)> method) {
    return createNotificationHandle(methodName, paramNames, method, std::index_sequence_for<ParamTypes...>{});
  }

  template <typename... ParamTypes>
  NotificationHandle GetHandle(const std::string &methodName, const NamedParamMapping &paramNames, std::function<void(ParamTypes...)> f) {
    return notificationHandle(methodName, paramNames, f);
  }

  template <typename... ParamTypes>
  NotificationHandle GetHandle(const std::string &methodName, const NamedParamMapping &paramNames, void (*f)(ParamTypes...)) {
    return GetHandle(methodName, paramNames, std::function<void(ParamTypes...)>(f));
  }

  inline NotificationHandle GetUncheckedNotificationHandle(std::function<void(const json&)> f) {
    NotificationHandle handle = [f](const json &params) -> void {
      f(params);
    };
    return handle;
  }

  template <typename T, typename ReturnType, typename... ParamTypes>
  MethodHandle methodHandle(const std::string &methodName, const NamedParamMapping &paramNames, ReturnType (T::*method)(ParamTypes...), T &instance) {
    std::function<ReturnType(ParamTypes...)> function = [&instance, method](ParamTypes &&... params) -> ReturnType {
      return (instance.*method)(std::forward<ParamTypes>(params)...);
    };
    return methodHandle(methodName, paramNames, function);
  }

  template <typename T, typename... ParamTypes>
  NotificationHandle notificationHandle(const std::string &methodName, const NamedParamMapping &paramNames, void (T::*method)(ParamTypes...), T &instance) {
    std::function<void(ParamTypes...)> function = [&instance, method](ParamTypes &&... params) -> void {
      return (instance.*method)(std::forward<ParamTypes>(params)...);
    };
    return notificationHandle(methodName, paramNames, function);
  }

  template <typename T, typename ReturnType, typename... ParamTypes>
  MethodHandle GetHandle(const std::string &methodName, const NamedParamMapping &paramNames, ReturnType (T::*method)(ParamTypes...), T &instance) {
    std::function<ReturnType(ParamTypes...)> function = [&instance, method](ParamTypes &&... params) -> ReturnType {
      return (instance.*method)(std::forward<ParamTypes>(params)...);
    };
    return GetHandle(methodName, paramNames, function);
  }

  template <typename T, typename... ParamTypes>
  NotificationHandle GetHandle(const std::string &methodName, const NamedParamMapping &paramNames, void (T::*method)(ParamTypes...), T &instance) {
    std::function<void(ParamTypes...)> function = [&instance, method](ParamTypes &&... params) -> void {
      return (instance.*method)(std::forward<ParamTypes>(params)...);
    };
    return GetHandle(methodName, paramNames, function);
  }
}
