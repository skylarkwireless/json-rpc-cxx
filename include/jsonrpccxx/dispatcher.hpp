#pragma once

#include "common.hpp"
#include "typemapper.hpp"
#include <cassert>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <sstream>
#include <vector>

namespace jsonrpccxx {

  class Dispatcher {
  public:
    Dispatcher() :
      methods(),
      notifications(),
      docstrings(),
      mapping(),
      paramTypes(),
      paramDocstrings()
    {}

    bool Add(const std::string &name, const std::string &docstring, MethodHandle callback, const NamedParamMapping &mapping = NAMED_PARAM_MAPPING) {
      if (Contains(name))
        return false;
      methods[name] = std::move(callback);
      docstrings[name] = docstring;
      if (!mapping.empty()) {
        this->mapping[name] = mapping;
      }
      return true;
    }

    inline bool Add(const std::string &name, MethodHandle callback, const NamedParamMapping &mapping = NAMED_PARAM_MAPPING) {
      return this->Add(name, "", callback, mapping);
    }

    bool Add(const std::string &name, const std::string &docstring, NotificationHandle callback, const NamedParamMapping &mapping = NAMED_PARAM_MAPPING) {
      if (Contains(name))
        return false;
      notifications[name] = std::move(callback);
      docstrings[name] = docstring;
      if (!mapping.empty()) {
        this->mapping[name] = mapping;
      }
      return true;
    }

    inline bool Add(const std::string &name, NotificationHandle callback, const NamedParamMapping &mapping = NAMED_PARAM_MAPPING) {
      return this->Add(name, "", callback, mapping);
    }

    template <typename ReturnType, typename... ParamTypes>
    bool Add(
      const std::string &name,
      const std::string &docstring,
      std::function<ReturnType(ParamTypes...)> method,
      const NamedParamMapping &args = NAMED_PARAM_MAPPING,
      const NamedParamMapping &argDocstrings = NAMED_PARAM_MAPPING)
    {
      if (Contains(name))
        return false;

      if (sizeof...(ParamTypes) != args.size()) {
        std::ostringstream errMsgStream;
        errMsgStream << "Error registering RPC method \"" << name << "\": number of listed parameters ("
                     << args.size() << ") does not match registered method's parameter list ("
                     << sizeof...(ParamTypes) << ").";

        throw std::invalid_argument(errMsgStream.str());
      }

      if((sizeof...(ParamTypes) != argDocstrings.size()) and not argDocstrings.empty()) {
        std::ostringstream errMsgStream;
        errMsgStream << "Error registering RPC method \"" << name << "\": number of listed parameters ("
                     << sizeof...(ParamTypes) << ") must match number of parameter docstrings ("
                     << argDocstrings.size() << "), or no docstrings must be provided.";
      }

      NamedParamMapping types;
      auto methodHandle = createMethodHandle(
        std::forward<decltype(method)>(method),
        std::index_sequence_for<ParamTypes...>{},
        types);

      methods[name] = std::move(methodHandle);
      docstrings[name] = docstring;
      if(!args.empty()) {
        mapping[name] = args;
        paramTypes[name] = std::move(types);
        paramDocstrings[name] = argDocstrings;
      }

      return true;
    }

    template <typename Func>
    inline bool Add(
      const std::string &name,
      const std::string &docstring,
      Func method,
      const NamedParamMapping &args = NAMED_PARAM_MAPPING,
      const NamedParamMapping &argDocstrings = NAMED_PARAM_MAPPING)
    {
      return this->Add(
        name,
        docstring,
        std::function(method),
        args,
        argDocstrings);
    }

    template <typename Class, typename ReturnType, typename... ParamTypes>
    inline bool Add(
      const std::string &name,
      const std::string &docstring,
      ReturnType (Class::*cb)(ParamTypes...),
      Class *cls,
      const NamedParamMapping &args = NAMED_PARAM_MAPPING,
      const NamedParamMapping &argDocstrings = NAMED_PARAM_MAPPING)
    {
      return this->Add(
        name,
        docstring,
        [cls, cb](ParamTypes&&... params) -> ReturnType
        {
          return std::mem_fn(cb)(cls, params...);
        },
        args,
        argDocstrings);
    }

    template <typename Class, typename ReturnType, typename... ParamTypes>
    inline bool Add(
      const std::string &name,
      const std::string &docstring,
      ReturnType (Class::*cb)(ParamTypes...) const,
      Class *cls,
      const NamedParamMapping &args = NAMED_PARAM_MAPPING,
      const NamedParamMapping &argDocstrings = NAMED_PARAM_MAPPING)
    {
      return this->Add(
        name,
        docstring,
        [cls, cb](ParamTypes&&... params) -> ReturnType
        {
          return std::mem_fn(cb)(cls, params...);
        },
        args,
        argDocstrings);
    }

    template <typename Func>
    bool Add(
      const std::string &name,
      const std::string &docstring,
      Func func,
      const ParamArgsMap &args)
    {
      if (name.find("rpc.", 0) == 0)
        return false;

      NamedParamMapping argNames{};
      NamedParamMapping argDocstrings{};

      for (const auto &[arg, doc]: args) {
        argNames.push_back(arg);
        argDocstrings.push_back(doc);
      }
      return this->Add(name, docstring, std::forward<Func>(func), argNames, argDocstrings);
    }

    template <typename Class, typename ReturnType, typename... ParamTypes>
    bool Add(
      const std::string &name,
      const std::string &docstring,
      ReturnType (Class::*cb)(ParamTypes...),
      Class *cls,
      const ParamArgsMap &args)
    {
      if (name.find("rpc.", 0) == 0)
        return false;

      NamedParamMapping argNames{};
      NamedParamMapping argDocstrings{};

      for (const auto &[arg, doc]: args) {
        argNames.push_back(arg);
        argDocstrings.push_back(doc);
      }
      return this->Add(name, docstring, cb, cls, argNames, argDocstrings);
    }

    // This workaround is necessary to avoid ambiguity between std::vector<std::string> and
    // std::map<std::string, std::string> when using braced initializer lists.
    template <typename Func>
    inline bool Add(
      const std::string &name,
      const std::string &docstring,
      Func func,
      const std::initializer_list<std::string> &args)
    {
      return this->Add(name, docstring, std::forward<Func>(func), NamedParamMapping(args));
    }

    // This workaround is necessary to avoid ambiguity between std::vector<std::string> and
    // std::map<std::string, std::string> when using braced initializer lists.
    template <typename Class, typename ReturnType, typename... ParamTypes>
    inline bool Add(
      const std::string &name,
      const std::string &docstring,
      ReturnType (Class::*cb)(ParamTypes...),
      Class *cls,
      const std::initializer_list<std::string> &args)
    {
      return this->Add(name, docstring, cb, cls, NamedParamMapping(args));
    }

    template <typename... ArgsType>
    void ForceAdd(
        const std::string &name,
        ArgsType&&... args)
    {
      this->Remove(name);
      const bool ret = this->Add(name, args...);

      (void)ret;
      assert(ret);
    }

    inline bool ContainsMethod(const std::string &name) const { return (methods.find(name) != methods.end()); }

    inline bool ContainsNotification(const std::string &name) const { return (notifications.find(name) != notifications.end()); }

    inline bool Contains(const std::string &name) const { return (ContainsMethod(name) || ContainsNotification(name)); }

    bool Remove(const std::string &name) {
      if (!Contains(name))
        return false;
      methods.erase(name);
      notifications.erase(name);
      mapping.erase(name);
      return true;
    }

    std::vector<std::string> MethodNames() const {
      std::vector<std::string> names;
      for(const auto &mapPair: methods) {
        names.push_back(mapPair.first);
      }
      return names;
    }

    std::vector<std::string> NotificationNames() const {
      std::vector<std::string> names;
      for(const auto &mapPair: notifications) {
        names.push_back(mapPair.first);
      }
      return names;
    }

    std::string MethodDocstring(const std::string &name) const {
      if (docstrings.count(name) > 0) {
          return docstrings.at(name);
      }
      return {};
    }

    NamedParamMapping MethodParamNames(const std::string &name) const {
      if (mapping.count(name) > 0) {
          return mapping.at(name);
      }
      return {};
    }

    NamedParamMapping MethodParamTypes(const std::string &name) const {
      auto paramTypeIter = paramTypes.find(name);
      if (paramTypeIter != paramTypes.end()) {
        return paramTypeIter->second;
      }
      return {};
    }

    NamedParamMapping MethodParamDocstrings(const std::string &name) const {
      auto paramDocstringIter = paramDocstrings.find(name);
      if (paramDocstringIter != paramDocstrings.end()) {
        return paramDocstringIter->second;
      }
      return {};
    }

    JsonRpcException process_type_error(const std::string &name, JsonRpcException &e) {
      if (e.Code() == -32602 && !e.Data().empty()) {
        std::string message = e.Message() + " for parameter ";
        if (this->mapping.find(name) != this->mapping.end()) {
          message += "\"" + mapping[name][e.Data().get<unsigned int>()] + "\"";
        } else {
          message += std::to_string(e.Data().get<unsigned int>());
        }
        return JsonRpcException(e.Code(), message);
      }
      return e;
    }

    json InvokeMethod(const std::string &name, const json &params) {
      auto method = methods.find(name);
      if (method == methods.end()) {
        throw JsonRpcException(method_not_found, "method not found: " + name);
      }
      try {
        return method->second(normalize_parameter(name, params));
      } catch (json::type_error &e) {
        throw JsonRpcException(invalid_params, "invalid parameter: " + std::string(e.what()));
      } catch (JsonRpcException &e) {
        throw process_type_error(name, e);
      }
    }

    void InvokeNotification(const std::string &name, const json &params) {
      auto notification = notifications.find(name);
      if (notification == notifications.end()) {
        throw JsonRpcException(method_not_found, "notification not found: " + name);
      }
      try {
        notification->second(normalize_parameter(name, params));
      } catch (json::type_error &e) {
        throw JsonRpcException(invalid_params, "invalid parameter: " + std::string(e.what()));
      } catch (JsonRpcException &e) {
        throw process_type_error(name, e);
      }
    }

  private:
    std::map<std::string, MethodHandle> methods;
    std::map<std::string, NotificationHandle> notifications;
    ParamArgsMap docstrings;
    std::map<std::string, NamedParamMapping> mapping;
    std::map<std::string, NamedParamMapping> paramTypes;
    std::map<std::string, NamedParamMapping> paramDocstrings;

    inline json normalize_parameter(const std::string &name, const json &params) {
      if (params.type() == json::value_t::array) {
        return params;
      } else if (params.type() == json::value_t::object) {
        if (mapping.find(name) == mapping.end()) {
          throw JsonRpcException(invalid_params, "invalid parameter: procedure doesn't support named parameter");
        }
        json result;
        for (auto const &p : mapping[name]) {
          if (params.find(p) == params.end()) {
            throw JsonRpcException(invalid_params, "invalid parameter: missing named parameter \"" + p + "\"");
          }
          result.push_back(params[p]);
        }
        return result;
      }
      throw JsonRpcException(invalid_request, "invalid request: params field must be an array, object");
    }
  };
} // namespace jsonrpccxx
