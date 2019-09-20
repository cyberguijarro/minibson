#pragma once

#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace microbson {
typedef unsigned char byte;

enum node_type {
  double_node   = 0x01,
  string_node   = 0x02,
  document_node = 0x03,
  array_node    = 0x04,
  binary_node   = 0x05,
  boolean_node  = 0x08,
  null_node     = 0x0A,
  int32_node    = 0x10,
  int64_node    = 0x12,
  unknown_node  = 0xFF
};

class document;

template <typename T>
struct type_converter {};

template <>
struct type_converter<double> {
  enum { node_type_code = double_node };
};

template <>
struct type_converter<std::string> {
  enum { node_type_code = string_node };
};

template <typename T>
struct type_converter<std::vector<T>> {
  enum { node_type_code = array_node };
};

template <>
struct type_converter<document> {
  enum { node_type_code = document_node };
};

template <>
struct type_converter<void *> {
  enum { node_type_code = binary_node };
};

template <>
struct type_converter<bool> {
  enum { node_type_code = boolean_node };
};

template <>
struct type_converter<int> {
  enum { node_type_code = int32_node };
};

template <>
struct type_converter<long long> {
  enum { node_type_code = int64_node };
};

struct node {
  byte *bytes_;

  node()
      : bytes_(NULL) {}

  node(byte *bytes)
      : bytes_(bytes) {}

  node_type get_type() const { return static_cast<node_type>(bytes_[0]); }

  const char *get_name() const {
    return reinterpret_cast<const char *>(bytes_ + 1);
  }

  size_t get_size() const {
    size_t result = 1U + strlen(get_name()) + 1U;

    switch (get_type()) {
    case double_node:
      result += sizeof(double);
      break;
    case document_node:
    case array_node:
      result += *reinterpret_cast<int *>(bytes_ + result);
      break;
    case string_node:
      result += (sizeof(int) + *reinterpret_cast<int *>(bytes_ + result));
      break;
    case binary_node:
      result += (sizeof(int) + *reinterpret_cast<int *>(bytes_ + result) + 1U);
      break;
    case boolean_node:
      result += 1U;
    case null_node:
      break;
    case int32_node:
      result += sizeof(int);
      break;
    case int64_node:
      result += sizeof(long long);
      break;
    default:
      result = 0U;
      break;
    }

    return result;
  }

  void *get_data() const { return bytes_ + 1U + strlen(get_name()) + 1U; }

  bool valid(size_t size) const {
    return (size >= 2) && (get_size() <= size) && get_size() != 0;
  }
};

class document {
private:
  byte *bytes_;

  bool lookup(const char *name, node &result) const {
    byte * iterator = bytes_ + sizeof(int);
    size_t left     = size() - sizeof(int);
    bool   found    = false;

    result = node(iterator);

    while (result.valid(left)) {
      if (strcmp(result.get_name(), name) == 0) {
        found = true;
        break;
      } else {
        iterator += result.get_size();
        left -= result.get_size();
        result = node(iterator);
      }
    }

    return found;
  }

  template <typename T, typename W>
  T get(node _node) const {
    return static_cast<T>(*reinterpret_cast<W *>(_node.get_data()));
  }

  std::string get_string(const node &_node) const {
    return std::string(reinterpret_cast<const char *>(_node.get_data()) +
                           sizeof(int),
                       *reinterpret_cast<int *>(_node.get_data()) - 1);
  }

  template <typename T, typename W>
  T get(const std::string &name, T _default) const {
    node _node;

    return lookup(name.c_str(), _node) ? get<T, W>(_node) : _default;
  }

  void dump(const node &_node, std::ostream &_stream) const {
    switch (_node.get_type()) {
    case double_node:
      _stream << get<double, double>(_node);
      break;
    case string_node:
      _stream << '"' << get_string(_node) << '"';
      break;
    case binary_node: {
      byte *             bytes = reinterpret_cast<byte *>(_node.get_data());
      std::ios::fmtflags flags(_stream.flags());

      _stream << std::hex << std::setw(2) << std::setfill('0');
      copy(bytes + 5U,
           bytes + 5U + *static_cast<int *>(_node.get_data()),
           std::ostream_iterator<int>(_stream));
      _stream.flags(flags);
      break;
    }
    case boolean_node:
      _stream << (get<bool, byte>(_node) ? "true" : "false");
      break;
    case null_node:
      _stream << "(null)";
      break;
    case int32_node:
      _stream << get<int, int>(_node);
      break;
    case int64_node:
      _stream << get<long long, long long>(_node);
      break;
    default:
      break;
    }
  }

public:
  document()
      : bytes_(NULL) {}

  document(void *bytes, size_t = 0)
      : bytes_(reinterpret_cast<byte *>(bytes)) {}

  bool valid() const {
    return bytes_ &&
           (*reinterpret_cast<int *>(bytes_) == static_cast<int>(size())) &&
           (size() >= 7U) && (bytes_[size() - 1] == 0);
  }

  size_t size() const {
    if (bytes_) {
      return *reinterpret_cast<int *>(bytes_);
    } else {
      return 0;
    }
  }

  double get(const std::string &name, double _default) const {
    return get<double, double>(name, _default);
  }

  std::string get(const std::string &name, const std::string &_default) const {
    std::string result;
    node        _node;

    bool found = lookup(name.c_str(), _node);
    if (found) {
      result = get_string(_node);
    } else {
      result = _default;
    }

    return result;
  }

  std::string get(const std::string &name, const char *default_) const {
    std::string result;
    node        node;

    bool found = lookup(name.c_str(), node);
    if (found) {
      result = get_string(node);
    } else {
      result = default_;
    }

    return result;
  }

  document get(const std::string &name, const document &_default) const {
    node     _node;
    bool     found = lookup(name.c_str(), _node);
    document result(_default);

    if (found) {
      result = document(_node.get_data());
    }

    return result;
  }

  std::pair<void *, size_t> get(const std::string &name) const {
    node                      _node;
    bool                      found = lookup(name.c_str(), _node);
    std::pair<void *, size_t> result(NULL, 0U);

    if (found) {
      result.second = *reinterpret_cast<int *>(_node.get_data());
      result.first  = reinterpret_cast<byte *>(_node.get_data()) + 5U;
    }

    return result;
  }

  bool get(const std::string &name, bool _default) const {
    return get<bool, byte>(name, _default);
  }

  int get(const std::string &name, int _default) const {
    return get<int, int>(name, _default);
  }

  long long get(const std::string &name, long long _default) const {
    return get<long long, long long>(name, _default);
  }

  void dump(std::ostream &_stream) const {
    byte * iterator = bytes_ + sizeof(int);
    size_t left     = size() - sizeof(int);
    node   _node(iterator);

    _stream << "{ ";

    while (_node.valid(left)) {
      _stream << _node.get_name() << " : ";

      if (_node.get_type() == document_node || _node.get_type() == array_node)
        document(_node.get_data(), *static_cast<int *>(_node.get_data()))
            .dump(_stream);
      else
        dump(_node, _stream);

      size_t size = _node.get_size();
      iterator += size;
      left -= size;
      _node = node(iterator);

      if (_node.valid(left))
        _stream << ", ";
    }

    _stream << " }";
  }

  /**\brief just return keys in the document
   * \todo add iterator and remove the method
   */
  std::list<std::string> keys() const {
    std::list<std::string> retval;

    byte * iterator = bytes_ + sizeof(int);
    size_t left     = size() - sizeof(int);
    node   node(iterator);
    while (node.valid(left)) {
      retval.emplace_back(node.get_name());
      size_t size = node.get_size();
      iterator += size;
      left -= size;
      node = microbson::node{iterator};
    }

    return retval;
  }

  bool contains(const std::string &name) const {
    node _node;

    return lookup(name.c_str(), _node);
  }

  template <typename T>
  bool contains(const std::string &name) const {
    node _node;
    bool found = lookup(name.c_str(), _node);

    return (found && (static_cast<int>(_node.get_type()) ==
                      static_cast<int>(type_converter<T>::node_type_code)));
  }
};
} // namespace microbson
