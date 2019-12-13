// minibson.hpp

#pragma once

#include "microbson.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#define MEMORY_ERROR "not enough memory in buffer"

namespace minibson {
using byte = uint8_t;

enum BsonNodeType {
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

// needed for prevent warning about enum compare
bool operator==(BsonNodeType lhs, int rhs) {
  return int(lhs) == rhs;
}
bool operator!=(BsonNodeType lhs, int rhs) {
  return int(lhs) != rhs;
}

class Document;
class Array;
class Binary;

template <class T>
struct type_traits {};

template <>
struct type_traits<double> {
  enum { node_type_code = double_node };
  using value_type = double;
};

template <>
struct type_traits<float> {
  enum { node_type_code = double_node };
  using value_type = double;
};

template <>
struct type_traits<int32_t> {
  enum { node_type_code = int32_node };
  using value_type = int32_t;
};

template <>
struct type_traits<int64_t> {
  enum { node_type_code = int64_node };
  using value_type = int64_t;
};

template <>
struct type_traits<long long int> {
  enum { node_type_code = int64_node };
  using value_type = int64_t;
};

template <>
struct type_traits<std::string> {
  enum { node_type_code = string_node };
  using value_type = std::string;
};

template <>
struct type_traits<std::string_view> {
  enum { node_type_code = string_node };
  using value_type = std::string;
};

template <>
struct type_traits<char *> {
  enum { node_type_code = string_node };
  using value_type = std::string;
};

template <>
struct type_traits<const char *> {
  enum { node_type_code = string_node };
  using value_type = std::string;
};

template <>
struct type_traits<bool> {
  enum { node_type_code = boolean_node };
  using value_type = bool;
};

template <>
struct type_traits<void> {
  enum { node_type_code = null_node };
  using value_type = void;
};

template <>
struct type_traits<Array> {
  enum { node_type_code = array_node };
  using value_type = Array;
};

template <>
struct type_traits<Document> {
  enum { node_type_code = document_node };
  using value_type = Document;
};

template <>
struct type_traits<Binary> {
  enum { node_type_code = binary_node };
  using value_type = Binary;
};

class NodeValue {
public:
  virtual ~NodeValue() = default;

  virtual BsonNodeType type() const noexcept = 0;
  /**\param buf where will be writed data
   * \param length max capacity of bytes for serialization
   * \return capacity of serialized bytes
   * \throw error if can not serialize the data
   */
  virtual int serialize(void *buf, int length) const noexcept(false) = 0;

  /**\return count of bytes, needed for serialization
   */
  virtual int getSerializedSize() const noexcept = 0;
};

using UNodeValue = std::unique_ptr<NodeValue>;

template <class T>
class NodeValueT final : public NodeValue {
public:
  using value_type = typename type_traits<T>::value_type;

  explicit NodeValueT(const T &val)
      : val_{val} {}
  explicit NodeValueT(T &&val)
      : val_{std::move(val)} {}

  BsonNodeType type() const noexcept override {
    return static_cast<BsonNodeType>(type_traits<T>::node_type_code);
  }

  const value_type &value() const noexcept { return val_; }
  value_type &      value() noexcept { return val_; }

  int getSerializedSize() const noexcept {
    if constexpr (std::is_same<value_type, std::string>::value) {
      return 4 /*size*/ + val_.size() + 1 /*\0*/;
    } else if constexpr (std::is_same<value_type, Array>::value ||
                         std::is_same<value_type, Document>::value ||
                         std::is_same<value_type, Binary>::value) {
      return val_.getSerializedSize();
    } else if constexpr (std::is_same<value_type, bool>::value) {
      return 1;
    } else {
      return sizeof(val_);
    }
  };

  int serialize(void *buf, int length) const override {
    if (length < getSerializedSize()) {
      throw bson::InvalidArgument{MEMORY_ERROR};
    }

    if constexpr (std::is_same<value_type, std::string>::value) {
      *reinterpret_cast<int *>(buf) = val_.size() + 1 /*\0*/;
      char *ptr = reinterpret_cast<char *>(buf) + 4 /*size*/;
      std::strcpy(ptr, val_.c_str());
      return 4 /*size*/ + val_.size() + 1 /*\0*/;
    } else if constexpr (std::is_same<value_type, Array>::value ||
                         std::is_same<value_type, Document>::value ||
                         std::is_same<value_type, Binary>::value) {
      return val_.serialize(buf, length);
    } else if constexpr (std::is_same<value_type, bool>::value) {
      *reinterpret_cast<byte *>(buf) = val_;
      return 1;
    } else {
      *reinterpret_cast<value_type *>(buf) = val_;
      return sizeof(val_);
    }
  }

private:
  value_type val_;
};

template <>
class NodeValueT<void> final : public NodeValue {
public:
  using value_type = typename type_traits<void>::value_type;

  NodeValueT() = default;

  BsonNodeType type() const noexcept override {
    return static_cast<BsonNodeType>(type_traits<void>::node_type_code);
  }
  int getSerializedSize() const noexcept override { return 0; }
  int serialize(void *, int) const override { return 0; }
};

class UNodeValueFactory {
public:
  template <class T>
  static UNodeValue create(T &&val) {
    return std::make_unique<NodeValueT<T>>(std::move(val));
  }

  template <class T>
  static UNodeValue create(const T &val) {
    return std::make_unique<NodeValueT<T>>(val);
  }

  static UNodeValue create() { return std::make_unique<NodeValueT<void>>(); }
};

class Binary final {
public:
  Binary() = default;
  Binary(const void *buf, int length) {
    buf_.resize(length);
    std::memcpy(buf_.data(), buf, length);
  }
  Binary(std::vector<byte> &&buf)
      : buf_(std::move(buf)) {}

  BsonNodeType type() const noexcept { return binary_node; }
  int          getSerializedSize() const noexcept {
    return 4 /*size*/ + 1 /*subtype*/ + buf_.size();
  }
  int serialize(void *buf, int bufSize) const {
    int size = buf_.size();
    if (bufSize < 4 /*size*/ + 1 /*subtype*/ + size) {
      throw bson::InvalidArgument{MEMORY_ERROR};
    }

    *reinterpret_cast<int *>(buf) = size;
    byte *ptr                     = reinterpret_cast<byte *>(buf) + 4 /*size*/;
    *ptr                          = '\0'; // binary subtype
    ++ptr;
    std::memcpy(ptr, buf_.data(), size);
    *(ptr + size) = '\0';
    return 4 /*size*/ + 1 /*subtype*/ + size;
  }

private:
  std::vector<byte> buf_;
};

class Document final {
public:
  Document() = default;

  /**\param buffer pointer to serialized bson document
   * \param length size of buffer, need for validate the document
   * \throw bson::InvalidArgument if can not deserialize bson
   */
  Document(const void *buffer, int length);

  BsonNodeType type() const noexcept { return document_node; }
  int          getSerializedSize() const noexcept {
    int count = 4 /*size*/;
    for (auto &[name, val] : doc_) {
      count += 1 /*type*/ + name.size() + 1 /*\0*/ + val->getSerializedSize();
    }
    return count + 1 /*\0*/;
  }

  /**\throw bson::InvalidArgument if memory not enough
   */
  int serialize(void *buf, int bufSize) const noexcept(false);

  /**\throw bson::OutOfRange if not have the value, or bson::BadCast if have not
   * same type
   */
  template <class ReturnType>
  const ReturnType &get(const std::string &key) const noexcept(false) {
    using value_type = typename type_traits<ReturnType>::value_type;
    static_assert(std::is_same<value_type, ReturnType>::value);

    if (auto found = doc_.find(key); found != doc_.end()) {
      if (type_traits<ReturnType>::node_type_code == found->second->type()) {
        return reinterpret_cast<const NodeValueT<value_type> *>(
                   found->second.get())
            ->value();
      } else {
        throw bson::BadCast{};
      }
    } else {
      throw bson::OutOfRange{"hame not value by key: " + std::string{key}};
    }
  }

  template <class ReturnType>
  ReturnType &get(const std::string &key) noexcept(false) {
    using value_type = typename type_traits<ReturnType>::value_type;
    static_assert(std::is_same<value_type, ReturnType>::value);

    if (auto found = doc_.find(key); found != doc_.end()) {
      if (found->second->type() == type_traits<ReturnType>::node_type_code) {
        return reinterpret_cast<NodeValueT<value_type> *>(found->second.get())
            ->value();
      } else {
        throw bson::BadCast{};
      }
    } else {
      throw bson::OutOfRange{"hame not value by key: " + std::string{key}};
    }
  }

  template <class InsertType>
  Document &set(std::string key, const InsertType &val) noexcept {
    doc_.insert_or_assign(std::move(key), UNodeValueFactory::create(val));
    return *this;
  }

  template <class InsertType>
  Document &set(std::string key, InsertType &&val) noexcept {
    doc_.insert_or_assign(std::move(key),
                          UNodeValueFactory::create(std::move(val)));
    return *this;
  }

  Document &set(std::string key) noexcept {
    doc_.insert_or_assign(std::move(key), UNodeValueFactory::create());
    return *this;
  }

  bool contains(const std::string &key) const noexcept {
    if (auto found = doc_.find(key); found != doc_.end()) {
      return true;
    }
    return false;
  }

  template <typename Type>
  bool contains(const std::string &key) const noexcept {
    if (auto found = doc_.find(key);
        found != doc_.end() &&
        type_traits<Type>::node_type_code == found->second->type()) {
      return true;
    }
    return false;
  }

  Document &erase(const std::string &key) noexcept(false) {
    doc_.erase(key);
    return *this;
  }

private:
  std::map<std::string, UNodeValue> doc_;
};

class Array final {
public:
  Array() = default;
  Array(const void *buf, int length);

  BsonNodeType type() const noexcept { return array_node; }
  int          getSerializedSize() const noexcept {
    int count = 4 /*size*/;
    for (size_t i = 0; i < arr_.size(); ++i) {
      count += 1 /*type*/ + std::to_string(i).size() + 1 /*\0*/ +
               arr_[i]->getSerializedSize();
    }
    return count + 1 /*\0*/;
  }
  int serialize(void *buf, int bufSize) const;

  void reserve(int n) noexcept { this->arr_.reserve(n); }

  /**\throw bson::OutOfRange or bson::BadCast
   */
  template <class ReturnType>
  const ReturnType &at(int i) const noexcept(false) {
    using value_type = typename type_traits<ReturnType>::value_type;
    static_assert(std::is_same<ReturnType, value_type>::value);

    if (arr_.size() < size_t(i)) {
      throw bson::OutOfRange{"have not value by index: " + std::to_string(i)};
    }

    if (arr_[i]->type() != type_traits<ReturnType>::value_type) {
      throw bson::BadCast{};
    }

    return reinterpret_cast<NodeValueT<value_type> *>(arr_[i].get())->value();
  }

  template <class ReturnType>
  ReturnType &at(int i) noexcept(false) {
    using value_type = typename type_traits<ReturnType>::value_type;
    static_assert(std::is_same<ReturnType, value_type>::value);

    if (arr_.size() < size_t(i)) {
      throw bson::OutOfRange{"have not value by index: " + std::to_string(i)};
    }

    if (arr_[i]->type() != type_traits<ReturnType>::value_type) {
      throw bson::BadCast{};
    }

    return reinterpret_cast<NodeValueT<value_type> *>(arr_[i].get())->value();
  }

  template <class InsertType>
  Array &push_back(InsertType &&val) {
    arr_.push_back(UNodeValueFactory::create(std::move(val)));
    return *this;
  }

  template <class InsertType>
  Array &push_back(const InsertType &val) {
    arr_.push_back(UNodeValueFactory::create(val));
    return *this;
  }

  /**\throw bson::OutOfRange
   */
  Array &erase(int i) noexcept(false) {
    if (arr_.size() > size_t(i)) {
      arr_.erase(arr_.begin() + i);
    }

    throw bson::OutOfRange{"index: " + std::to_string(i) +
                           " - more then size: " + std::to_string(arr_.size())};
  }

private:
  std::vector<UNodeValue> arr_;
};

Document::Document(const void *buffer, int length) {
  microbson::Document doc{buffer, length};
  for (auto i = doc.begin(); i != doc.end(); ++i) {
    microbson::Node node = *i;
    switch (node.type()) {
    case microbson::string_node:
      doc_.emplace(node.name(),
                   UNodeValueFactory::create(node.value<std::string_view>()));
      break;
    case microbson::boolean_node:
      doc_.emplace(node.name(), UNodeValueFactory::create(node.value<bool>()));
      break;
    case microbson::int32_node:
      doc_.emplace(node.name(),
                   UNodeValueFactory::create(node.value<int32_t>()));
      break;
    case microbson::int64_node:
      doc_.emplace(node.name(),
                   UNodeValueFactory::create(node.value<int64_t>()));
      break;
    case microbson::double_node:
      doc_.emplace(node.name(),
                   UNodeValueFactory::create(node.value<double>()));
      break;
    case microbson::null_node:
      doc_.emplace(node.name(), UNodeValueFactory::create());
      break;
    case microbson::array_node:
      doc_.emplace(
          node.name(),
          UNodeValueFactory::create(Array{node.data(), node.length()}));
      break;
    case microbson::document_node:
      doc_.emplace(
          node.name(),
          UNodeValueFactory::create(Document{node.data(), node.length()}));
      break;
    case microbson::binary_node:
      doc_.emplace(
          node.name(),
          UNodeValueFactory::create(Binary{node.data(), node.length()}));
      break;
    case microbson::unknown_node:
      throw bson::InvalidArgument{"unknown node by key: " +
                                  std::string{node.name()}};
      break;
    }
  }
}

int Document::serialize(void *buf, int length) const {
  int size = this->getSerializedSize();

  if (length < size) {
    throw bson::InvalidArgument{MEMORY_ERROR};
  }

  *reinterpret_cast<int *>(buf) = size;
  char *ptr                     = reinterpret_cast<char *>(buf);
  int   offset                  = 4 /*size*/;
  for (auto &[name, val] : doc_) {
    // serialize type and name
    *(ptr + offset) = val->type();
    ++offset;
    std::strcpy(ptr + offset, name.c_str());
    offset += name.size() + 1;

    offset += val->serialize(ptr + offset, length - offset - 1 /*\0*/);
  }

  *(ptr + offset) = '\0';
  ++offset;

  if (offset != size) { // have to be same as getSerializedSize
    throw std::runtime_error{"invalid serialization"}; // TODO is it needed?
  }

  return offset;
}

Array::Array(const void *buffer, int length) {
  microbson::Document doc{buffer, length};
  arr_.reserve(doc.size());
  for (auto i = doc.begin(); i != doc.end(); ++i) {
    microbson::Node node = *i;
    switch (node.type()) {
    case microbson::string_node:
      arr_.emplace_back(
          UNodeValueFactory::create(node.value<std::string_view>()));
      break;
    case microbson::boolean_node:
      arr_.emplace_back(UNodeValueFactory::create(node.value<bool>()));
      break;
    case microbson::int32_node:
      arr_.emplace_back(UNodeValueFactory::create(node.value<int32_t>()));
      break;
    case microbson::int64_node:
      arr_.emplace_back(UNodeValueFactory::create(node.value<int64_t>()));
      break;
    case microbson::double_node:
      arr_.emplace_back(UNodeValueFactory::create(node.value<double>()));
      break;
    case microbson::null_node:
      arr_.emplace_back(UNodeValueFactory::create());
      break;
    case microbson::array_node:
      arr_.emplace_back(
          UNodeValueFactory::create(Array{node.data(), node.length()}));
      break;
    case microbson::document_node:
      arr_.emplace_back(
          UNodeValueFactory::create(Document{node.data(), node.length()}));
      break;
    case microbson::binary_node:
      arr_.emplace_back(
          UNodeValueFactory::create(Binary{node.data(), node.length()}));
      break;
    case microbson::unknown_node:
      throw bson::InvalidArgument{"unknown node by index: " +
                                  std::string{node.name()}};
      break;
    }
  }
}

int Array::serialize(void *buf, int length) const {
  int size = this->getSerializedSize();

  if (length < size) {
    throw bson::InvalidArgument{MEMORY_ERROR};
  }

  *reinterpret_cast<int *>(buf) = size;
  char *ptr                     = reinterpret_cast<char *>(buf);
  int   offset                  = 4 /*size*/;
  for (size_t i = 0; i < arr_.size(); ++i) {
    std::string       name = std::to_string(i);
    const UNodeValue &val  = arr_[i];

    // serialize type and name
    *(ptr + offset) = val->type();
    ++offset;
    std::strcpy(ptr + offset, name.c_str());
    offset += name.size() + 1;

    offset += val->serialize(ptr + offset, length - offset - 1 /*\0*/);
  }

  *(ptr + offset) = '\0';
  ++offset;

  if (offset != size) { // have to be same as getSerializedSize
    throw std::runtime_error{"invalid serialization"}; // TODO is it needed?
  }

  return offset;
}
} // namespace minibson
