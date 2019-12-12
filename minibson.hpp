#pragma once

#include <cstring>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace minibson {

// Basic types

enum bson_node_type {
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

// for prevent enum compare warning
bool operator==(bson_node_type lhs, int rhs) {
  return int(lhs) == int(rhs);
}

bool operator!=(bson_node_type lhs, int rhs) {
  return int(lhs) != int(rhs);
}

template <typename T>
struct type_converter {};

class node;

using node_t = std::unique_ptr<minibson::node>;

class node {
public:
  node() = default;
  virtual ~node() {}
  virtual void   serialize(void *const buffer, const size_t count) const = 0;
  virtual size_t get_serialized_size() const                             = 0;
  virtual bson_node_type get_node_code() const                           = 0;
  virtual node_t         copy() const                                    = 0;
  virtual void           dump(std::ostream &) const                      = 0;
  virtual void           dump(std::ostream &stream, int) const { dump(stream); }
  static node_t
  create(bson_node_type type, const void *const buffer, const size_t count);

  node(const node &) = delete;
  node &operator=(const node &) = delete;
  node &operator=(node &&) = default;
};

// Value types

class null : public node {
public:
  null() {}

  null(const void *const, const size_t) {}

  void serialize(void *const, const size_t) const override {}

  size_t get_serialized_size() const override { return 0; }

  bson_node_type get_node_code() const override { return null_node; }

  node_t copy() const override { return node_t{}; }

  void dump(std::ostream &stream) const override { stream << "null"; };
};

template <typename T, bson_node_type N>
class scalar : public node {
private:
  T value_;

public:
  scalar(const T value)
      : value_(value) {}

  scalar(const void *const buffer, const size_t count = 0) {
    (void)count;
    value_ = *reinterpret_cast<const T *>(buffer);
  };

  void serialize(void *const buffer, const size_t count = 0) const override {
    (void)count;
    *reinterpret_cast<T *>(buffer) = value_;
  }

  size_t get_serialized_size() const override { return sizeof(T); }

  bson_node_type get_node_code() const override { return N; }

  node_t copy() const override { return node_t{new scalar<T, N>(value_)}; }

  void dump(std::ostream &stream) const override { stream << value_; };

  const T &get_value() const { return value_; }
};

class int32 : public scalar<int, int32_node> {
public:
  int32(const int value)
      : scalar<int, int32_node>(value) {}

  int32(const void *const buffer, const size_t count)
      : scalar<int, int32_node>(buffer, count){};
};

template <>
struct type_converter<int> {
  enum { node_type_code = int32_node };
  typedef int32 node_class;
};

class int64 : public scalar<long long int, int64_node> {
public:
  int64(const long long int value)
      : scalar<long long int, int64_node>(value) {}

  int64(const void *const buffer, const size_t count)
      : scalar<long long int, int64_node>(buffer, count){};
};

template <>
struct type_converter<long long int> {
  enum { node_type_code = int64_node };
  typedef int64 node_class;
};

class Double : public scalar<double, double_node> {
public:
  Double(const double value)
      : scalar<double, double_node>(value) {}

  Double(const void *const buffer, const size_t count)
      : scalar<double, double_node>(buffer, count){};
};

template <>
struct type_converter<double> {
  enum { node_type_code = double_node };
  typedef Double node_class;
};

class string : public node {
private:
  std::string value_;

public:
  explicit string(const std::string &value)
      : value_(value) {}
  explicit string(std::string &&value)
      : value_{std::move(value)} {}

  string(const void *const buffer, const size_t count) {
    if (count >= 5) {
      const size_t max    = count - sizeof(unsigned int);
      const size_t actual = *reinterpret_cast<const unsigned int *>(buffer);

      value_.assign(reinterpret_cast<const char *>(buffer) +
                        sizeof(unsigned int),
                    std::min(actual, max) - 1);
    }
  };

  void serialize(void *const buffer, const size_t count) const override {
    *reinterpret_cast<unsigned int *>(buffer) = value_.length() + 1;
    std::memcpy(reinterpret_cast<char *>(buffer) + sizeof(unsigned int),
                value_.c_str(),
                value_.length());
    *(reinterpret_cast<char *>(buffer) + count - 1) = '\0';
  }

  size_t get_serialized_size() const override {
    return sizeof(unsigned int) + value_.length() + 1;
  }

  bson_node_type get_node_code() const override { return string_node; }

  node_t copy() const override { return node_t{new string(value_)}; }

  void dump(std::ostream &stream) const override {
    stream << "\"" << value_ << "\"";
  };

  const std::string &get_value() const { return value_; }
};

template <>
struct type_converter<std::string> {
  enum { node_type_code = string_node };
  typedef string node_class;
};

class boolean : public node {
private:
  bool value_;

public:
  explicit boolean(const bool value)
      : value_(value) {}

  boolean(const void *const buffer, const size_t count = 0) {
    (void)count;
    switch (*reinterpret_cast<const unsigned char *>(buffer)) {
    case 1:
      value_ = true;
      break;
    default:
      value_ = false;
      break;
    }
  };

  void serialize(void *const buffer, const size_t count = 0) const override {
    (void)count;
    *reinterpret_cast<unsigned char *>(buffer) = value_ ? true : false;
  }

  size_t get_serialized_size() const override { return 1; }

  bson_node_type get_node_code() const override { return boolean_node; }

  node_t copy() const override { return node_t{new boolean(value_)}; }

  void dump(std::ostream &stream) const override {
    stream << (value_ ? "true" : "false");
  };

  const bool &get_value() const { return value_; }
};

template <>
struct type_converter<bool> {
  enum { node_type_code = boolean_node };
  typedef boolean node_class;
};

class binary : public node {
public:
  struct buffer {
    buffer()                  = default;
    buffer(const buffer &rhs) = default;
    buffer &operator=(const buffer &rhs) = default;
    buffer(buffer &&rhs)
        : data{std::move(rhs.data)} {
      rhs.data = {};
    }
    buffer &operator=(buffer &&rhs) {
      this->data = std::move(rhs.data);
      rhs.data   = {};
      return *this;
    }
    explicit buffer(std::vector<uint8_t> &&rhs)
        : data{rhs} {}

    buffer(const void *buf, size_t length)
        : data(length) {
      std::memcpy(data.data(), buf, length);
    }

    void dump(std::ostream &stream) const {
      stream << "<binary: " << data.size() << " bytes>";
    };

    std::vector<uint8_t> data;
  };

private:
  buffer value_;

public:
  explicit binary(const buffer &buf)
      : value_{buf} {}

  explicit binary(buffer &&buf)
      : value_{std::move(buf)} {}

  /**\brief if flag create set to true, then create new buffer from input data
   * (buffer, count). Otherwise deserialize bson data from buffer
   */
  binary(const void *const buf, const size_t count, const bool create = false)
      : value_{} {
    const unsigned char *byte_buffer =
        reinterpret_cast<const unsigned char *>(buf);

    if (create) {
      value_.data.resize(count);
      std::memcpy(value_.data.data(), byte_buffer, value_.data.size());
    } else {
      size_t length = *reinterpret_cast<const int *>(byte_buffer);
      value_.data.resize(length);
      std::memcpy(value_.data.data(), byte_buffer + 5, length);
    }
  };

  void serialize(void *const buf, const size_t count = 0) const override {
    (void)count;
    unsigned char *byte_buffer = reinterpret_cast<unsigned char *>(buf);

    *reinterpret_cast<int *>(byte_buffer) = value_.data.size();
    std::memcpy(
        byte_buffer + sizeof(uint) + 1, value_.data.data(), value_.data.size());
  }

  size_t get_serialized_size() const override {
    return sizeof(uint) + 1 + value_.data.size();
  }

  bson_node_type get_node_code() const override { return binary_node; }

  node_t copy() const override { return node_t{new binary(value_)}; }

  void dump(std::ostream &stream) const override { value_.dump(stream); };

  const buffer &get_value() const { return value_; }
};

template <>
struct type_converter<binary::buffer> {
  enum { node_type_code = binary_node };
  typedef binary node_class;
};

// Composite types

class element_list
    : protected std::map<std::string, node_t>
    , public node {
public:
  typedef std::map<std::string, node_t>::const_iterator const_iterator;

  element_list() {}

  element_list(const element_list &other)
      : std::map<std::string, node_t>{}
      , node{} {
    for (const_iterator i = other.begin(); i != other.end(); i++) {
      this->emplace(i->first, i->second->copy());
    }
  }

  element_list(element_list &&rhs)
      : std::map<std::string, node_t>{std::move(rhs)} {}

  element_list &operator=(element_list &&rhs) = default;

  element_list(const void *const buffer, const size_t count) {
    const unsigned char *byte_buffer =
        reinterpret_cast<const unsigned char *>(buffer);
    size_t position = 0;

    while (position < count) {
      bson_node_type type =
          static_cast<bson_node_type>(byte_buffer[position++]);
      std::string name(reinterpret_cast<const char *>(byte_buffer + position));

      position += name.length() + 1;
      node_t curNode =
          node::create(type, byte_buffer + position, count - position);

      if (curNode != nullptr) {
        position += curNode->get_serialized_size();
        this->emplace(name, std::move(curNode));
      } else
        break;
    }
  }

  void serialize(void *const buffer, const size_t count) const {
    unsigned char *byte_buffer = reinterpret_cast<unsigned char *>(buffer);
    int            position    = 0;

    for (const_iterator i = begin(); i != end(); i++) {
      // Header
      byte_buffer[position] = i->second->get_node_code();
      position++;
      // Key
      std::strcpy(reinterpret_cast<char *>(byte_buffer + position),
                  i->first.c_str());
      position += i->first.length() + 1;
      // Value
      i->second->serialize(byte_buffer + position, count - position);
      position += i->second->get_serialized_size();
    }
  }

  size_t get_serialized_size() const {
    size_t result = 0;

    for (const_iterator i = begin(); i != end(); i++)
      result += 1 + i->first.length() + 1 + i->second->get_serialized_size();

    return result;
  }

  void dump(std::ostream &stream) const {
    stream << "{ ";

    for (const_iterator i = begin(); i != end(); i++) {
      stream << "\"" << i->first << "\": ";
      i->second->dump(stream);

      if (std::next(i) != end())
        stream << ", ";
    }

    stream << " }";
  }

  void dump(std::ostream &stream, const int level) const {
    stream << "{ ";

    for (const_iterator i = begin(); i != end(); i++) {
      stream << std::endl;

      for (int j = 0; j < level + 1; j++)
        stream << "\t";

      stream << "\"" << i->first << "\": ";
      i->second->dump(stream, level + 1);

      if (++i != end())
        stream << ", ";
      --i;
    }

    stream << std::endl;

    for (int j = 0; j < level; j++)
      stream << "\t";

    stream << "}";
  }

  const_iterator begin() const {
    return std::map<std::string, node_t>::begin();
  }

  const_iterator end() const { return std::map<std::string, node_t>::end(); }

  bool contains(const std::string &key) const {
    return (std::map<std::string, node_t>::find(key) != end());
  }

  template <typename T>
  bool contains(const std::string &key) const {
    const_iterator position = std::map<std::string, node_t>::find(key);
    return (position != end()) && (position->second->get_node_code() ==
                                   type_converter<T>::node_type_code);
  }

  ~element_list() {}
};

class array;

class document : public element_list {
public:
  document() = default;

  document(const document &) = default;
  document(document &&)      = default;

  document &operator=(document &&) = default;

  document(const void *const buffer, [[maybe_unused]] const size_t count = 0)
      : element_list(reinterpret_cast<const unsigned char *>(buffer) + 4,
                     *reinterpret_cast<const int *>(buffer) - 4 - 1) {}

  void serialize(void *const buffer, const size_t count) const override {
    size_t serialized_size = get_serialized_size();

    if (count >= serialized_size) {
      unsigned char *byte_buffer = reinterpret_cast<unsigned char *>(buffer);

      *reinterpret_cast<int *>(buffer) = serialized_size;
      element_list::serialize(byte_buffer + 4, count - 4 - 1);
      byte_buffer[4 + element_list::get_serialized_size()] = 0;
    }
  }

  std::vector<uint8_t> serialize() const {
    std::vector<uint8_t> retval(this->get_serialized_size());
    *reinterpret_cast<int *>(retval.data()) = retval.size();
    element_list::serialize(retval.data() + 4, retval.size() - 4 - 1);
    return retval;
  }

  size_t get_serialized_size() const override {
    return 4 + element_list::get_serialized_size() + 1;
  }

  bson_node_type get_node_code() const override { return document_node; }

  node_t copy() const override { return node_t{new document(*this)}; }

  template <typename result_type>
  const result_type &get(const std::string &key,
                         const result_type &_default) const {
    const bson_node_type node_type_code = static_cast<bson_node_type>(
        type_converter<result_type>::node_type_code);
    typedef typename type_converter<result_type>::node_class node_class;

    auto founded = this->find(key);
    if ((founded != end()) &&
        (founded->second->get_node_code() == node_type_code))
      return reinterpret_cast<const node_class *>(founded->second.get())
          ->get_value();
    else
      return _default;
  }

  const document &get(const std::string &key, const document &_default) const {
    auto founded = this->find(key);
    if ((founded != end()) &&
        (founded->second->get_node_code() == document_node))
      return *reinterpret_cast<const document *>(founded->second.get());
    else
      return _default;
  }

  const array &get(const std::string &key, const array &_default) const {
    auto founded = this->find(key);
    if ((founded != end()) && (founded->second->get_node_code() == array_node))
      return *reinterpret_cast<const array *>(founded->second.get());
    else
      return _default;
  }

  /**\return by value, because here _default type is pointer to char, so we can
   * not return reference
   */
  std::string get(const std::string &key, const char *_default) const {
    auto founded = this->find(key);
    if ((founded != end()) && (founded->second->get_node_code() == string_node))
      return reinterpret_cast<const string *>(founded->second.get())
          ->get_value();
    else
      return std::string(_default);
  }

  template <typename value_type>
  document &set(const std::string &key, const value_type &value) {
    typedef typename type_converter<value_type>::node_class node_class;

    this->erase(key);
    this->emplace(key, node_t{new node_class(value)});
    return (*this);
  }

  document &set(const std::string &key, const char *value) {
    this->erase(key);
    this->emplace(key, node_t{new string(value)});
    return (*this);
  }

  document &set(const std::string &key, const document &value) {
    this->erase(key);
    this->emplace(key, value.copy());
    return (*this);
  }

  document &set(const std::string &key) {
    this->erase(key);
    this->emplace(key, node_t{new null()});
    return (*this);
  }

  document &set(const std::string &key, document &&value) {
    this->erase(key);
    this->emplace(key,
                  std::unique_ptr<document>(new document(std::move(value))));
    return (*this);
  }

  document &set(const std::string &key, array &&value);

  document &set(const std::string &key, std::string &&value) {
    this->erase(key);
    this->emplace(key, std::unique_ptr<string>(new string(std::move(value))));
    return (*this);
  }

  document &set(const std::string &key, binary::buffer &&value) {
    this->erase(key);
    this->emplace(key, std::unique_ptr<binary>(new binary(std::move(value))));
    return (*this);
  }
};

class array
    : protected std::vector<node_t>
    , public node {
public:
  array() = default;

  array(const array &rhs)
      : std::vector<node_t>{}
      , node{} {
    std::vector<node_t>::reserve(rhs.size());
    for (auto i = rhs.begin(); i != rhs.end(); i++) {
      this->emplace_back((*i)->copy());
    }
  }

  array(array &&rhs)
      : std::vector<node_t>{std::move(rhs)} {};

  array(const void *const buffer, size_t count) {
    const unsigned char *byte_buffer =
        reinterpret_cast<const unsigned char *>(buffer) + 4;
    size_t deserializeCount = count - 4 - 1;
    size_t position         = 0;

    while (position < deserializeCount) {
      bson_node_type type =
          static_cast<bson_node_type>(byte_buffer[position++]);
      std::string name(reinterpret_cast<const char *>(byte_buffer + position));

      position += name.length() + 1;
      node_t curNode = node::create(
          type, byte_buffer + position, deserializeCount - position);

      if (curNode != nullptr) {
        position += curNode->get_serialized_size();
        std::vector<node_t>::emplace_back(std::move(curNode));
      } else
        break;
    }
  }

  size_t size() const { return std::vector<node_t>::size(); }

  virtual void serialize(void *const  buffer,
                         const size_t count) const override {
    size_t serialized_size = get_serialized_size();

    if (count >= serialized_size) {
      *reinterpret_cast<int *>(buffer) = serialized_size;

      int serializeCount = count - 4 - 1;

      unsigned char *byte_buffer =
          reinterpret_cast<unsigned char *>(buffer) + 4;
      int position = 0;

      int key = 0;
      for (const_iterator i = std::vector<node_t>::begin();
           i != std::vector<node_t>::end();
           ++i, ++key) {
        // Header
        byte_buffer[position] = (*i)->get_node_code();
        position++;
        // Key
        std::string keyS = std::to_string(key);
        std::strcpy(reinterpret_cast<char *>(byte_buffer + position),
                    keyS.c_str());
        position += keyS.length() + 1;
        // Value
        (*i)->serialize(byte_buffer + position, serializeCount - position);
        position += (*i)->get_serialized_size();
      }

      byte_buffer[this->get_serialized_size()] = 0;
    }
  }

  virtual size_t get_serialized_size() const override {
    size_t result = 4 + 1; // 4 for size and 1 is last

    int count = 0;
    for (auto i = std::vector<node_t>::begin(); i != std::vector<node_t>::end();
         ++i, ++count) {
      result +=
          1 + std::to_string(count).length() + 1 + (*i)->get_serialized_size();
    }

    return result;
  }

  virtual void dump(std::ostream &stream) const override {
    stream << "[ ";

    for (auto i = std::vector<node_t>::begin(); i != std::vector<node_t>::end();
         ++i) {
      (*i)->dump(stream);

      if (std::next(i) != std::vector<node_t>::end())
        stream << ", ";
    }

    stream << " ]";
  }

  node_t copy() const override { return node_t{new array(*this)}; }

  bson_node_type get_node_code() const override { return array_node; }

  template <typename result_type>
  result_type &at(int i) {
    const bson_node_type node_type_code = static_cast<bson_node_type>(
        type_converter<result_type>::node_type_code);
    typedef typename type_converter<result_type>::node_class node_class;

    node_t &curNode = std::vector<node_t>::at(i);
    if (curNode->get_node_code() != node_type_code) {
      throw std::bad_cast{};
    }
    return reinterpret_cast<node_class *>(curNode.get())->get_value();
  }

  template <typename result_type>
  const result_type &at(int i) const {
    const bson_node_type node_type_code = static_cast<bson_node_type>(
        type_converter<result_type>::node_type_code);
    typedef typename type_converter<result_type>::node_class node_class;

    const node_t &curNode = std::vector<node_t>::at(i);
    if (curNode->get_node_code() != node_type_code) {
      throw std::bad_cast{};
    }
    return reinterpret_cast<const node_class *>(curNode.get())->get_value();
  }

  // we need copy the elements because it have to return array, not document
  template <typename value_type>
  array &push_back(const value_type &value) {
    typedef typename type_converter<value_type>::node_class node_class;

    std::vector<node_t>::emplace_back(node_t{new node_class(value)});
    return (*this);
  }

  template <typename value_type>
  array &push_back(value_type &&value) {
    typedef typename type_converter<value_type>::node_class node_class;

    std::vector<node_t>::emplace_back(node_t{new node_class(std::move(value))});
    return (*this);
  }

  array &push_back(const char *value) {
    std::vector<node_t>::emplace_back(node_t{new string(value)});
    return (*this);
  }

  array &push_back() {
    std::vector<node_t>::emplace_back(node_t{new null()});
    return (*this);
  }
};

inline document &document::set(const std::string &key, array &&value) {
  this->erase(key);
  this->emplace(key, std::unique_ptr<array>(new array(std::move(value))));
  return *this;
}

template <>
struct type_converter<document> {
  enum { node_type_code = document_node };
  typedef document node_class;
};

template <>
struct type_converter<array> {
  enum { node_type_code = array_node };
  typedef array node_class;
};

inline node_t node::create(bson_node_type    type,
                           const void *const buffer,
                           const size_t      count) {
  switch (type) {
  case null_node:
    return node_t{new null()};
  case int32_node:
    return node_t{new int32(buffer, count)};
  case int64_node:
    return node_t{new int64(buffer, count)};
  case double_node:
    return node_t{new Double(buffer, count)};
  case document_node:
    return node_t{new document(buffer, count)};
  case array_node:
    return node_t{new array(buffer, count)};
  case string_node:
    return node_t{new string(buffer, count)};
  case binary_node:
    return node_t{new binary(buffer, count)};
  case boolean_node:
    return node_t{new boolean(buffer, count)};
  default:
    return NULL;
  }
}
} // namespace minibson
