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

class Document;
class Array;
class Binary;

template <class T>
struct type_traits {};

template <>
struct type_traits<double> {
  enum { node_type_code = bson::double_node };
  using value_type  = double;
  using return_type = double;
};

template <>
struct type_traits<float> {
  enum { node_type_code = bson::double_node };
  using value_type  = double;
  using return_type = float;
};

template <>
struct type_traits<int32_t> {
  enum { node_type_code = bson::int32_node };
  using value_type  = int32_t;
  using return_type = int32_t;
};

template <>
struct type_traits<int64_t> {
  enum { node_type_code = bson::int64_node };
  using value_type  = int64_t;
  using return_type = int64_t;
};

template <>
struct type_traits<long long int> {
  enum { node_type_code = bson::int64_node };
  using value_type  = int64_t;
  using return_type = int64_t;
};

template <>
struct type_traits<std::string> {
  enum { node_type_code = bson::string_node };
  using value_type  = std::string;
  using return_type = std::string;
};

template <>
struct type_traits<std::string_view> {
  enum { node_type_code = bson::string_node };
  using value_type  = std::string;
  using return_type = std::string_view;
};

template <>
struct type_traits<const char *> {
  enum { node_type_code = bson::string_node };
  using value_type  = std::string;
  using return_type = const char *;

  static return_type converter(const value_type &value) {
    return value.c_str();
  }
};

template <>
struct type_traits<bool> {
  enum { node_type_code = bson::boolean_node };
  using value_type  = bool;
  using return_type = bool;
};

template <>
struct type_traits<void> {
  enum { node_type_code = bson::null_node };
  using value_type  = void;
  using return_type = void;
};

template <>
struct type_traits<Array> {
  enum { node_type_code = bson::array_node };
  using value_type  = Array;
  using return_type = Array;
};

template <>
struct type_traits<Document> {
  enum { node_type_code = bson::document_node };
  using value_type  = Document;
  using return_type = Document;
};

template <>
struct type_traits<Binary> {
  enum { node_type_code = bson::binary_node };
  using value_type  = Binary;
  using return_type = Binary;
};

/**\brief Special Case for get some scalar value as double
 */
template <>
struct type_traits<bson::Scalar> {
  using value_type  = bson::Scalar;
  using return_type = double;
};

class NodeValue {
public:
  virtual ~NodeValue() = default;

  virtual bson::NodeType type() const noexcept = 0;
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
  using value_type = T;

  explicit NodeValueT(const T &val) noexcept
      : val_{val} {
    // prevent wrong values types
    static_assert(
        std::is_same<T, double>::value || std::is_same<T, std::string>::value ||
        std::is_same<T, Document>::value || std::is_same<T, Array>::value ||
        std::is_same<T, Binary>::value || std::is_same<T, bool>::value ||
        std::is_same<T, int32_t>::value || std::is_same<T, int64_t>::value);
  }
  explicit NodeValueT(T &&val) noexcept
      : val_{std::move(val)} {
    // prevent wrong values types
    static_assert(
        std::is_same<T, double>::value || std::is_same<T, std::string>::value ||
        std::is_same<T, Document>::value || std::is_same<T, Array>::value ||
        std::is_same<T, Binary>::value || std::is_same<T, bool>::value ||
        std::is_same<T, int32_t>::value || std::is_same<T, int64_t>::value);
  }

  inline bson::NodeType type() const noexcept override {
    constexpr bson::NodeType retval =
        static_cast<bson::NodeType>(type_traits<T>::node_type_code);
    return retval;
  }

  const value_type &value() const noexcept { return val_; }
  value_type &      value() noexcept { return val_; }

  int getSerializedSize() const noexcept override {
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
  using value_type = void;

  NodeValueT() noexcept = default;

  bson::NodeType type() const noexcept override {
    bson::NodeType retval =
        static_cast<bson::NodeType>(type_traits<void>::node_type_code);
    return retval;
  }
  int getSerializedSize() const noexcept override { return 0; }
  int serialize(void *, int) const override { return 0; }
};

class UNodeValueFactory {
public:
  template <class InputType,
            typename = typename std::enable_if<
                std::is_rvalue_reference<InputType &&>::value>::type>
  static UNodeValue create(InputType &&val) noexcept {
    using value_type  = typename type_traits<InputType>::value_type;
    using return_type = typename type_traits<InputType>::return_type;

    if constexpr (std::is_same<InputType, value_type>::value) {
      return std::make_unique<NodeValueT<value_type>>(std::move(val));
    } else if constexpr (std::is_convertible<InputType, value_type>::value) {
      return std::make_unique<NodeValueT<value_type>>(val);
    } else if constexpr (std::is_nothrow_constructible<InputType,
                                                       value_type>::value) {
      return std::make_unique<NodeValueT<value_type>>(value_type(val));
    } else {
      constexpr value_type (*back_converter)(const return_type &) =
          type_traits<InputType>::back_converter;

      return std::make_unique<NodeValueT<value_type>>(back_converter(val));
    }
  }

  template <class InputType>
  static UNodeValue create(const InputType &val) noexcept {
    using value_type  = typename type_traits<InputType>::value_type;
    using return_type = typename type_traits<InputType>::return_type;

    if constexpr (std::is_convertible<InputType, value_type>::value) {
      return std::make_unique<NodeValueT<value_type>>(val);
    } else if constexpr (std::is_nothrow_constructible<InputType,
                                                       value_type>::value) {
      return std::make_unique<NodeValueT<value_type>>(value_type(val));
    } else {
      constexpr value_type (*back_converter)(const return_type &) =
          type_traits<InputType>::back_converter;

      return std::make_unique<NodeValueT<value_type>>(back_converter(val));
    }
  }

  static UNodeValue create() { return std::make_unique<NodeValueT<void>>(); }
};

class Binary final {
public:
  Binary() noexcept = default;
  Binary(const void *buf, int length) noexcept {
    buf_.resize(length);
    std::memcpy(buf_.data(), buf, length);
  }
  Binary(std::vector<byte> &&buf) noexcept
      : buf_(std::move(buf)) {}

  explicit Binary(microbson::Binary b) noexcept {
    this->buf_.resize(b.second);
    std::memcpy(this->buf_.data(), b.first, b.second);
  }

  Binary(const Binary &)     = delete;
  Binary(Binary &&) noexcept = default;

  constexpr bson::NodeType type() const noexcept { return bson::binary_node; }
  inline int               getSerializedSize() const noexcept {
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

  std::vector<byte> buf_;
};

class Document final {
public:
  Document() noexcept = default;

  /**\param buffer pointer to serialized bson document
   * \param length size of buffer, need for validate the document
   * \throw bson::InvalidArgument if can not deserialize bson
   */
  Document(const void *buffer, int length) {
    microbson::Document doc{buffer, length};
    this->deserialize(doc);
  }
  explicit Document(microbson::Document doc) { this->deserialize(doc); }

  Document(const Document &)        = delete;
  Document(Document &&rhs) noexcept = default;
  Document &operator=(Document &&) noexcept = default;

  constexpr bson::NodeType type() const noexcept { return bson::document_node; }
  int                      getSerializedSize() const noexcept {
    int count = 4 /*size*/;
    for (auto &[key, val] : doc_) {
      count += 1 /*type*/ + key.size() + 1 /*\0*/ + val->getSerializedSize();
    }
    return count + 1 /*\0*/;
  }

  /**\throw bson::InvalidArgument if memory not enough
   * \brief serialize in existing buffer
   */
  int serialize(void *buf, int bufSize) const noexcept(false);

  /**\brief create new buffer, serialize in it, and return it
   * \return new buffer with serialized bson document
   */
  std::vector<byte> serialize() const noexcept(false);

  /**\throw bson::OutOfRange if not have the value, or bson::BadCast if have not
   * same type
   */
  template <
      class InputType,
      typename = typename std::enable_if<
          std::is_same<typename type_traits<InputType>::return_type,
                       typename type_traits<InputType>::value_type>::value &&
          !std::is_fundamental<InputType>::value>::type>
  const typename type_traits<InputType>::return_type &
  get(const std::string &key) const noexcept(false) {
    using value_type           = typename type_traits<InputType>::value_type;
    constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

    if (auto found = doc_.find(key); found != doc_.end()) {
      if (found->second->type() == nodeTypeCode) {
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

  template <class InputType,
            typename = typename std::enable_if<std::is_same<
                typename type_traits<InputType>::return_type,
                typename type_traits<InputType>::value_type>::value>::type>
  typename type_traits<InputType>::return_type &
  get(const std::string &key) noexcept(false) {
    using value_type           = typename type_traits<InputType>::value_type;
    constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

    if (auto found = doc_.find(key); found != doc_.end()) {
      if (found->second->type() == nodeTypeCode) {
        return reinterpret_cast<NodeValueT<value_type> *>(found->second.get())
            ->value();
      } else {
        throw bson::BadCast{};
      }
    } else {
      throw bson::OutOfRange{"hame not value by key: " + std::string{key}};
    }
  }

  template <
      class InputType,
      typename = typename std::enable_if<
          !std::is_same<typename type_traits<InputType>::return_type,
                        typename type_traits<InputType>::value_type>::value ||
          std::is_fundamental<InputType>::value>::type>
  typename type_traits<InputType>::return_type get(const std::string &key) const
      noexcept(false) {
    using value_type           = typename type_traits<InputType>::value_type;
    using return_type          = typename type_traits<InputType>::return_type;
    constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

    if (auto found = doc_.find(key); found != doc_.end()) {
      if (found->second->type() == nodeTypeCode) {
        if constexpr (std::is_convertible<value_type, return_type>::value) {
          return reinterpret_cast<const NodeValueT<value_type> *>(
                     found->second.get())
              ->value();
        } else if constexpr (std::is_nothrow_constructible<return_type,
                                                           value_type>::value) {
          return return_type{reinterpret_cast<const NodeValueT<value_type> *>(
                                 found->second.get())
                                 ->value()};
        } else {
          constexpr return_type (*converter)(const value_type &) =
              type_traits<InputType>::converter;

          return converter(reinterpret_cast<const NodeValueT<value_type> *>(
                               found->second.get())
                               ->value());
        }
      } else {
        throw bson::BadCast{};
      }
    } else {
      throw bson::OutOfRange{"hame not value by key: " + std::string{key}};
    }
  }

  template <class InsertType,
            typename = typename std::enable_if<
                !std::is_convertible<InsertType, const char *>::value>::type>
  Document &set(std::string_view key, const InsertType &val) noexcept {
    doc_.insert_or_assign(std::string{key}, UNodeValueFactory::create(val));
    return *this;
  }

  template <class InsertType,
            typename = typename std::enable_if<
                std::is_rvalue_reference<InsertType &&>::value &&
                !std::is_convertible<InsertType, const char *>::value>::type>
  Document &set(std::string_view key, InsertType &&val) noexcept {
    doc_.insert_or_assign(std::string{key},
                          UNodeValueFactory::create(std::move(val)));
    return *this;
  }

  /**\brief for c-string
   */
  template <class InsertType,
            typename = typename std::enable_if<
                std::is_convertible<InsertType, const char *>::value>::type>
  Document &set(std::string_view key, InsertType val) noexcept {
    doc_.insert_or_assign(
        std::string{key},
        UNodeValueFactory::create(reinterpret_cast<const char *>(val)));
    return *this;
  }

  Document &set(std::string_view key) noexcept {
    doc_.insert_or_assign(std::string{key}, UNodeValueFactory::create());
    return *this;
  }

  template <class InputType, class InsertType>
  Document &set(std::string key, const InsertType &val) noexcept {
    using value_type  = typename type_traits<InputType>::value_type;
    using return_type = typename type_traits<InputType>::return_type;

    if constexpr (std::is_nothrow_constructible<value_type,
                                                InsertType>::value) {
      doc_.insert_or_assign(std::move(key),
                            UNodeValueFactory::create(value_type(val)));
    } else {
      constexpr value_type (*back_converter)(const return_type &) =
          type_traits<InputType>::back_converter;

      doc_.insert_or_assign(std::move(key),
                            UNodeValueFactory::create(back_converter(val)));
    }

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
    constexpr int nodeTypeCode = type_traits<Type>::node_type_code;

    if (auto found = doc_.find(key);
        found != doc_.end() && found->second->type() == nodeTypeCode) {
      return true;
    }
    return false;
  }

  Document &erase(const std::string &key) noexcept(false) {
    doc_.erase(key);
    return *this;
  }

  class Iterator {
    friend Document;
    using imp_iter_type = std::map<std::string, UNodeValue>::iterator;

  public:
    Iterator() noexcept = default;

    Iterator &operator++() noexcept {
      ++imp_;
      return *this;
    }
    Iterator &operator++(int) noexcept {
      ++imp_;
      return *this;
    }
    Iterator &operator--() noexcept {
      --imp_;
      return *this;
    }
    Iterator &operator--(int) noexcept {
      --imp_;
      return *this;
    }

    UNodeValue &operator*() noexcept { return imp_->second; }
    bool        operator==(const Iterator &rhs) const noexcept {
      return this->imp_ == rhs.imp_;
    }
    bool operator!=(const Iterator &rhs) const noexcept {
      return this->imp_ != rhs.imp_;
    }

    inline bson::NodeType type() const noexcept { return imp_->second->type(); }
    inline const std::string &key() const noexcept { return imp_->first; }

    /**\throw bson::BadCast
     */
    template <
        class InputType,
        typename = typename std::enable_if<
            std::is_same<typename type_traits<InputType>::return_type,
                         typename type_traits<InputType>::value_type>::value &&
            !std::is_fundamental<InputType>::value>::type>
    typename type_traits<InputType>::return_type &value() const
        noexcept(false) {
      using value_type           = typename type_traits<InputType>::value_type;
      constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

      if (imp_->second->type() == nodeTypeCode) {
        return reinterpret_cast<const NodeValueT<value_type> *>(
                   imp_->second.get())
            ->value();
      }

      throw bson::BadCast{};
    }

    template <
        class InputType,
        typename = typename std::enable_if<
            !std::is_same<typename type_traits<InputType>::return_type,
                          typename type_traits<InputType>::value_type>::value ||
            std::is_fundamental<InputType>::value>::type>
    typename type_traits<InputType>::return_type value() const noexcept(false) {
      using value_type           = typename type_traits<InputType>::value_type;
      using return_type          = typename type_traits<InputType>::return_type;
      constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

      if (imp_->second->type() == nodeTypeCode) {
        if constexpr (std::is_convertible<value_type, return_type>::value) {
          return reinterpret_cast<const NodeValueT<value_type> *>(
                     imp_->second.get())
              ->value();
        } else if constexpr (std::is_nothrow_constructible<return_type,
                                                           value_type>::value) {
          return return_type(reinterpret_cast<const NodeValueT<value_type> *>(
                                 imp_->second.get())
                                 ->value());
        } else {
          constexpr return_type (*converter)(const value_type &) =
              type_traits<InputType>::converter;

          return converter(reinterpret_cast<const NodeValueT<value_type> *>(
                               imp_->second.get())
                               ->value());
        }
      }

      throw bson::BadCast{};
    }

  private:
    Iterator(imp_iter_type &&imp) noexcept
        : imp_{imp} {}

  private:
    imp_iter_type imp_;
  };

  class ConstIterator {
    friend Document;
    using imp_iter_type = std::map<std::string, UNodeValue>::const_iterator;

  public:
    ConstIterator() noexcept = default;

    ConstIterator &operator++() noexcept {
      ++imp_;
      return *this;
    }
    ConstIterator &operator++(int) noexcept {
      ++imp_;
      return *this;
    }
    ConstIterator &operator--() noexcept {
      --imp_;
      return *this;
    }
    ConstIterator &operator--(int) noexcept {
      --imp_;
      return *this;
    }

    const UNodeValue &operator*() const noexcept { return imp_->second; }
    bool              operator==(const Iterator &rhs) const noexcept {
      return this->imp_ == rhs.imp_;
    }
    bool operator!=(const Iterator &rhs) const noexcept {
      return this->imp_ != rhs.imp_;
    }

    inline bson::NodeType type() const noexcept { return imp_->second->type(); }
    inline const std::string &key() const noexcept { return imp_->first; }

    /**\throw bson::BadCast
     */
    template <
        class InputType,
        typename = typename std::enable_if<
            std::is_same<typename type_traits<InputType>::return_type,
                         typename type_traits<InputType>::value_type>::value &&
            !std::is_fundamental<InputType>::value>::type>
    const typename type_traits<InputType>::return_type &value() const
        noexcept(false) {
      using value_type           = typename type_traits<InputType>::value_type;
      constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

      if (imp_->second->type() == nodeTypeCode) {
        return reinterpret_cast<const NodeValueT<value_type> *>(
                   imp_->second.get())
            ->value();
      }

      throw bson::BadCast{};
    }

    template <
        class InputType,
        typename = typename std::enable_if<
            !std::is_same<typename type_traits<InputType>::return_type,
                          typename type_traits<InputType>::value_type>::value ||
            std::is_fundamental<InputType>::value>::type>
    typename type_traits<InputType>::return_type value() const noexcept(false) {
      using value_type           = typename type_traits<InputType>::value_type;
      using return_type          = typename type_traits<InputType>::return_type;
      constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

      if (imp_->second->type() == nodeTypeCode) {
        if constexpr (std::is_convertible<value_type, return_type>::value) {
          return reinterpret_cast<const NodeValueT<value_type> *>(
                     imp_->second.get())
              ->value();
        } else if constexpr (std::is_nothrow_constructible<return_type,
                                                           value_type>::value) {
          return return_type(reinterpret_cast<const NodeValueT<value_type> *>(
                                 imp_->second.get())
                                 ->value());
        } else {
          constexpr return_type (*converter)(const value_type &) =
              type_traits<InputType>::converter;

          return converter(reinterpret_cast<const NodeValueT<value_type> *>(
                               imp_->second.get())
                               ->value());
        }
      }

      throw bson::BadCast{};
    }

  private:
    ConstIterator(imp_iter_type &&imp) noexcept
        : imp_{imp} {}

  private:
    imp_iter_type imp_;
  };

  Iterator      begin() noexcept { return Iterator{doc_.begin()}; }
  Iterator      end() noexcept { return Iterator{doc_.end()}; }
  ConstIterator begin() const noexcept { return ConstIterator{doc_.begin()}; }
  ConstIterator end() const noexcept { return ConstIterator{doc_.end()}; }

private:
  void deserialize(microbson::Document doc);

private:
  std::map<std::string, UNodeValue> doc_;
};

class Array final {
public:
  Array() noexcept = default;
  Array(const void *buffer, int length) {
    microbson::Document doc{buffer, length};
    this->deserialize(doc);
  }
  explicit Array(microbson::Array doc) { this->deserialize(doc); }

  Array(const Array &)     = delete;
  Array(Array &&) noexcept = default;
  Array &operator=(Array &&) noexcept = default;

  constexpr bson::NodeType type() const noexcept { return bson::array_node; }
  int                      getSerializedSize() const noexcept {
    int count = 4 /*size*/;
    for (size_t i = 0; i < arr_.size(); ++i) {
      count += 1 /*type*/ + std::to_string(i).size() + 1 /*\0*/ +
               arr_[i]->getSerializedSize();
    }
    return count + 1 /*\0*/;
  }

  /**\brief serialize in existing buffer
   * \return capacity of serialized bytes
   */
  int serialize(void *buf, int bufSize) const noexcept(false);

  /**\brief create new buffer, serialize in it, and return it
   * \return new buffer with serialized bson array
   */
  std::vector<byte> serialize() const noexcept(false);

  void reserve(int n) noexcept { this->arr_.reserve(n); }

  int size() const { return arr_.size(); }

  /**\throw bson::OutOfRange or bson::BadCast
   */
  template <
      class InputType,
      typename = typename std::enable_if<
          std::is_same<typename type_traits<InputType>::return_type,
                       typename type_traits<InputType>::value_type>::value &&
          !std::is_fundamental<InputType>::value>::type>
  const typename type_traits<InputType>::return_type &at(int i) const
      noexcept(false) {
    using value_type           = typename type_traits<InputType>::value_type;
    constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

    if (arr_.size() < size_t(i)) {
      throw bson::OutOfRange{"have not value by index: " + std::to_string(i)};
    }

    if (arr_[i]->type() != nodeTypeCode) {
      throw bson::BadCast{};
    }

    return reinterpret_cast<const NodeValueT<value_type> *>(arr_[i].get())
        ->value();
  }

  template <class InputType,
            typename = typename std::enable_if<std::is_same<
                typename type_traits<InputType>::return_type,
                typename type_traits<InputType>::value_type>::value>::type>
  typename type_traits<InputType>::return_type &at(int i) noexcept(false) {
    using value_type           = typename type_traits<InputType>::value_type;
    constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

    if (arr_.size() < size_t(i)) {
      throw bson::OutOfRange{"have not value by index: " + std::to_string(i)};
    }

    if (arr_[i]->type() != nodeTypeCode) {
      throw bson::BadCast{};
    }

    return reinterpret_cast<NodeValueT<value_type> *>(arr_[i].get())->value();
  }

  template <
      class InputType,
      typename = typename std::enable_if<
          !std::is_same<typename type_traits<InputType>::return_type,
                        typename type_traits<InputType>::value_type>::value ||
          std::is_fundamental<InputType>::value>::type>
  typename type_traits<InputType>::return_type at(int i) const noexcept(false) {
    using value_type           = typename type_traits<InputType>::value_type;
    using return_type          = typename type_traits<InputType>::return_type;
    constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

    if (arr_.size() < size_t(i)) {
      throw bson::OutOfRange{"have not value by index: " + std::to_string(i)};
    }

    if (arr_[i]->type() != nodeTypeCode) {
      throw bson::BadCast{};
    }

    if constexpr (std::is_convertible<value_type, return_type>::value) {
      return reinterpret_cast<const NodeValueT<value_type> *>(arr_[i].get())
          ->value();
    } else if constexpr (std::is_nothrow_constructible<return_type,
                                                       value_type>::value) {
      return return_type(
          reinterpret_cast<const NodeValueT<value_type> *>(arr_[i].get())
              ->value());
    } else {
      constexpr return_type (*converter)(const value_type &) =
          type_traits<InputType>::converter;

      return converter(
          reinterpret_cast<const NodeValueT<value_type> *>(arr_[i].get())
              ->value());
    }
  }

  template <class InsertType,
            typename = typename std::enable_if<
                std::is_rvalue_reference<InsertType &&>::value &&
                !std::is_convertible<InsertType, const char *>::value>::type>
  Array &push_back(InsertType &&val) {
    arr_.emplace_back(UNodeValueFactory::create(std::move(val)));
    return *this;
  }

  template <class InsertType,
            typename = typename std::enable_if<
                !std::is_convertible<InsertType, const char *>::value>::type>
  Array &push_back(const InsertType &val) {
    arr_.emplace_back(UNodeValueFactory::create(val));
    return *this;
  }

  /**\brief for c-string
   */
  template <class InsertType,
            typename = typename std::enable_if<
                std::is_convertible<InsertType, const char *>::value>::type>
  Array &push_back(InsertType val) {
    arr_.emplace_back(
        UNodeValueFactory::create(reinterpret_cast<const char *>(val)));
    return *this;
  }

  template <class InputType, class InsertType>
  Array &push_back(const InsertType &val) noexcept {
    using value_type  = typename type_traits<InputType>::value_type;
    using return_type = typename type_traits<InputType>::return_type;

    if constexpr (std::is_nothrow_constructible<value_type,
                                                InsertType>::value) {
      arr_.emplace_back(UNodeValueFactory::create(value_type(val)));
    } else {
      constexpr value_type (*back_converter)(const return_type &) =
          type_traits<InputType>::back_converter;

      arr_.emplace_back(UNodeValueFactory::create(back_converter(val)));
    }

    return *this;
  }

  Array &push_back() {
    arr_.emplace_back(UNodeValueFactory::create());
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

  class Iterator {
    friend Array;
    using imp_iter_type = std::vector<UNodeValue>::iterator;

  public:
    Iterator() noexcept = default;

    Iterator &operator++() {
      ++imp_;
      ++num_;
      return *this;
    }
    Iterator &operator++(int) {
      ++imp_;
      ++num_;
      return *this;
    }
    Iterator &operator--() {
      --imp_;
      --num_;
      return *this;
    }
    Iterator &operator--(int) {
      --imp_;
      --num_;
      return *this;
    }

    UNodeValue &operator*() noexcept { return *imp_; }
    bool        operator==(const Iterator &rhs) const noexcept {
      return this->imp_ == rhs.imp_;
    }
    bool operator!=(const Iterator &rhs) const noexcept {
      return this->imp_ != rhs.imp_;
    }

    inline bson::NodeType type() const noexcept { return (*imp_)->type(); }
    inline std::string    key() const noexcept { return std::to_string(num_); }

    /**\throw bson::BadCast
     */
    template <
        class InputType,
        typename = typename std::enable_if<
            std::is_same<typename type_traits<InputType>::return_type,
                         typename type_traits<InputType>::value_type>::value &&
            !std::is_fundamental<InputType>::value>::type>
    typename type_traits<InputType>::return_type &value() const
        noexcept(false) {
      using value_type           = typename type_traits<InputType>::value_type;
      constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

      if ((*imp_)->type() == nodeTypeCode) {
        return reinterpret_cast<const NodeValueT<value_type> *>((*imp_).get())
            ->value();
      }

      throw bson::BadCast{};
    }

    template <
        class InputType,
        typename = typename std::enable_if<
            !std::is_same<typename type_traits<InputType>::return_type,
                          typename type_traits<InputType>::value_type>::value ||
            std::is_fundamental<InputType>::value>::type>
    typename type_traits<InputType>::return_type value() const noexcept(false) {
      using value_type           = typename type_traits<InputType>::value_type;
      using return_type          = typename type_traits<InputType>::return_type;
      constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

      if ((*imp_)->type() == nodeTypeCode) {
        if constexpr (std::is_convertible<value_type, return_type>::value) {
          return reinterpret_cast<const NodeValueT<value_type> *>((*imp_).get())
              ->value();
        } else if constexpr (std::is_nothrow_constructible<return_type,
                                                           value_type>::value) {
          return return_type(
              reinterpret_cast<const NodeValueT<value_type> *>((*imp_).get())
                  ->value());
        } else {
          constexpr return_type (*converter)(const value_type &) =
              type_traits<InputType>::converter;

          return converter(
              reinterpret_cast<const NodeValueT<value_type> *>((*imp_).get())
                  ->value());
        }
      }

      throw bson::BadCast{};
    }

  private:
    Iterator(imp_iter_type &&imp, size_t num) noexcept
        : imp_{imp}
        , num_{num} {}

  private:
    imp_iter_type imp_;
    size_t        num_;
  };

  class ConstIterator {
    friend Array;
    using imp_iter_type = const UNodeValue *;

  public:
    ConstIterator() noexcept = default;

    ConstIterator &operator++() noexcept {
      ++num_;
      return *this;
    }
    ConstIterator &operator++(int) noexcept {
      ++num_;
      return *this;
    }
    ConstIterator &operator--() noexcept {
      --num_;
      return *this;
    }
    ConstIterator &operator--(int) noexcept {
      --num_;
      return *this;
    }

    const UNodeValue &operator*() const noexcept { return *(imp_ + num_); }
    bool              operator==(const ConstIterator &rhs) const noexcept {
      return this->imp_ + this->num_ == rhs.imp_ + rhs.num_;
    }
    bool operator!=(const ConstIterator &rhs) const noexcept {
      return this->imp_ + this->num_ != rhs.imp_ + rhs.num_;
    }

    inline bson::NodeType type() const noexcept {
      return (*(imp_ + num_))->type();
    }
    inline std::string key() const noexcept { return std::to_string(num_); }

    /**\throw bson::BadCast
     */
    template <
        class InputType,
        typename = typename std::enable_if<
            std::is_same<typename type_traits<InputType>::return_type,
                         typename type_traits<InputType>::value_type>::value &&
            !std::is_fundamental<InputType>::value>::type>
    const typename type_traits<InputType>::return_type &value() const
        noexcept(false) {
      using value_type           = typename type_traits<InputType>::value_type;
      constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

      if ((*(imp_ + num_))->type() == nodeTypeCode) {
        return reinterpret_cast<const NodeValueT<value_type> *>((*imp_).get())
            ->value();
      }

      throw bson::BadCast{};
    }

    template <
        class InputType,
        typename = typename std::enable_if<
            !std::is_same<typename type_traits<InputType>::return_type,
                          typename type_traits<InputType>::value_type>::value ||
            std::is_fundamental<InputType>::value>::type>
    typename type_traits<InputType>::return_type value() const noexcept(false) {
      using value_type           = typename type_traits<InputType>::value_type;
      using return_type          = typename type_traits<InputType>::return_type;
      constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

      if ((*(imp_ + num_))->type() == nodeTypeCode) {
        if constexpr (std::is_convertible<value_type, return_type>::value) {
          return reinterpret_cast<const NodeValueT<value_type> *>((*imp_).get())
              ->value();
        } else if constexpr (std::is_nothrow_constructible<return_type,
                                                           value_type>::value) {
          return return_type(
              reinterpret_cast<const NodeValueT<value_type> *>((*imp_).get())
                  ->value());
        } else {
          constexpr return_type (*converter)(const value_type &) =
              type_traits<InputType>::converter;

          return converter(
              reinterpret_cast<const NodeValueT<value_type> *>((*imp_).get())
                  ->value());
        }
      }

      throw bson::BadCast{};
    }

  private:
    ConstIterator(imp_iter_type imp, size_t num) noexcept
        : imp_{imp}
        , num_{num} {}

  private:
    imp_iter_type imp_;
    size_t        num_;
  };
  Iterator      begin() noexcept { return Iterator{arr_.begin(), 0}; }
  Iterator      end() noexcept { return Iterator{arr_.end(), arr_.size()}; }
  ConstIterator begin() const noexcept { return ConstIterator{arr_.data(), 0}; }
  ConstIterator end() const noexcept {
    return ConstIterator{arr_.data(), arr_.size()};
  }

private:
  void deserialize(microbson::Document doc) noexcept(false);

private:
  std::vector<UNodeValue> arr_;
};

inline void Document::deserialize(microbson::Document doc) {
  for (microbson::Node node : doc) {
    switch (node.type()) {
    case bson::string_node:
      doc_.emplace(node.key(),
                   UNodeValueFactory::create(node.value<std::string_view>()));
      break;
    case bson::boolean_node:
      doc_.emplace(node.key(), UNodeValueFactory::create(node.value<bool>()));
      break;
    case bson::int32_node:
      doc_.emplace(node.key(),
                   UNodeValueFactory::create(node.value<int32_t>()));
      break;
    case bson::int64_node:
      doc_.emplace(node.key(),
                   UNodeValueFactory::create(node.value<int64_t>()));
      break;
    case bson::double_node:
      doc_.emplace(node.key(), UNodeValueFactory::create(node.value<double>()));
      break;
    case bson::null_node:
      doc_.emplace(node.key(), UNodeValueFactory::create());
      break;
    case bson::array_node:
      doc_.emplace(
          node.key(),
          UNodeValueFactory::create(Array{node.value<microbson::Array>()}));
      break;
    case bson::document_node:
      doc_.emplace(node.key(),
                   UNodeValueFactory::create(
                       Document{node.value<microbson::Document>()}));
      break;
    case bson::binary_node:
      doc_.emplace(
          node.key(),
          UNodeValueFactory::create(Binary{node.value<microbson::Binary>()}));
      break;
    default:
      throw bson::InvalidArgument{"unknown node by key: " +
                                  std::string{node.key()}};
      break;
    }
  }
}

inline int Document::serialize(void *buf, int length) const {
  int size = this->getSerializedSize();

  if (length < size) {
    throw bson::InvalidArgument{MEMORY_ERROR};
  }

  *reinterpret_cast<int *>(buf) = size;
  char *ptr                     = reinterpret_cast<char *>(buf);
  int   offset                  = 4 /*size*/;
  for (auto &[key, val] : doc_) {
    // serialize type and key
    *(ptr + offset) = val->type();
    ++offset;
    std::strcpy(ptr + offset, key.c_str());
    offset += key.size() + 1;

    offset += val->serialize(ptr + offset, length - offset - 1 /*\0*/);
  }

  *(ptr + offset) = '\0';
  ++offset;

  if (offset != size) { // have to be same as getSerializedSize
    throw std::runtime_error{"invalid serialization"}; // TODO is it needed?
  }

  return offset;
}

inline std::vector<byte> Document::serialize() const {
  int               size = this->getSerializedSize();
  std::vector<byte> retval(size);
  this->serialize(retval.data(), size);
  return retval;
}

inline void Array::deserialize(microbson::Document doc) {
  arr_.reserve(doc.size());
  for (microbson::Node node : doc) {
    switch (node.type()) {
    case bson::string_node:
      arr_.emplace_back(
          UNodeValueFactory::create(node.value<std::string_view>()));
      break;
    case bson::boolean_node:
      arr_.emplace_back(UNodeValueFactory::create(node.value<bool>()));
      break;
    case bson::int32_node:
      arr_.emplace_back(UNodeValueFactory::create(node.value<int32_t>()));
      break;
    case bson::int64_node:
      arr_.emplace_back(UNodeValueFactory::create(node.value<int64_t>()));
      break;
    case bson::double_node:
      arr_.emplace_back(UNodeValueFactory::create(node.value<double>()));
      break;
    case bson::null_node:
      arr_.emplace_back(UNodeValueFactory::create());
      break;
    case bson::array_node:
      arr_.emplace_back(
          UNodeValueFactory::create(Array{node.value<microbson::Array>()}));
      break;
    case bson::document_node:
      arr_.emplace_back(UNodeValueFactory::create(
          Document{node.value<microbson::Document>()}));
      break;
    case bson::binary_node:
      arr_.emplace_back(
          UNodeValueFactory::create(Binary{node.value<microbson::Binary>()}));
      break;
    default:
      throw bson::InvalidArgument{"unknown node by index: " +
                                  std::string{node.key()}};
      break;
    }
  }
}

inline int Array::serialize(void *buf, int length) const {
  int size = this->getSerializedSize();

  if (length < size) {
    throw bson::InvalidArgument{MEMORY_ERROR};
  }

  *reinterpret_cast<int *>(buf) = size;
  char *ptr                     = reinterpret_cast<char *>(buf);
  int   offset                  = 4 /*size*/;
  for (size_t i = 0; i < arr_.size(); ++i) {
    std::string       key = std::to_string(i);
    const UNodeValue &val = arr_[i];

    // serialize type and key
    *(ptr + offset) = val->type();
    ++offset;
    std::strcpy(ptr + offset, key.c_str());
    offset += key.size() + 1;

    offset += val->serialize(ptr + offset, length - offset - 1 /*\0*/);
  }

  *(ptr + offset) = '\0';
  ++offset;

  if (offset != size) { // have to be same as getSerializedSize
    throw std::runtime_error{"invalid serialization"}; // TODO is it needed?
  }

  return offset;
}

inline std::vector<byte> Array::serialize() const {
  int               size = this->getSerializedSize();
  std::vector<byte> retval(size);

  this->serialize(retval.data(), size);

  return retval;
}

/**\brief special case if we need get some number and we don't care about type
 * of it
 */
template <>
inline typename type_traits<bson::Scalar>::return_type
Document::get<bson::Scalar>(const std::string &key) const noexcept(false) {
  if (auto found = doc_.find(key); found != doc_.end()) {
    const NodeValue *node = found->second.get();
    switch (node->type()) {
    case bson::double_node:
      return reinterpret_cast<const NodeValueT<double> *>(node)->value();
    case bson::int32_node:
      return reinterpret_cast<const NodeValueT<int32_t> *>(node)->value();
    case bson::int64_node:
      return reinterpret_cast<const NodeValueT<int64_t> *>(node)->value();
    default:
      throw bson::BadCast{};
    }
  } else {
    throw bson::OutOfRange{"have not value by key: " + key};
  }
}
/**\brief special case if we need get some number and we don't care about type
 * of it
 */
template <>
inline typename type_traits<bson::Scalar>::return_type
Array::at<bson::Scalar>(int i) const noexcept(false) {
  if (size_t(i) >= arr_.size()) {
    throw bson::OutOfRange{"have not value by index: " + std::to_string(i)};
  }

  const NodeValue *node = arr_[i].get();
  switch (node->type()) {
  case bson::double_node:
    return reinterpret_cast<const NodeValueT<double> *>(node)->value();
  case bson::int32_node:
    return reinterpret_cast<const NodeValueT<int32_t> *>(node)->value();
  case bson::int64_node:
    return reinterpret_cast<const NodeValueT<int64_t> *>(node)->value();
  default:
    throw bson::BadCast{};
  }
}

template <>
inline bool Document::contains<bson::Scalar>(const std::string &key) const
    noexcept {
  if (auto found = doc_.find(key); found != doc_.end()) {
    if (auto type = found->second->type(); type == bson::double_node ||
                                           type == bson::int32_node ||
                                           type == bson::int64_node) {
      return true;
    }
  }
  return false;
}
} // namespace minibson

namespace std {
template <>
struct iterator_traits<minibson::Document::ConstIterator> {
  using iterator_category = std::bidirectional_iterator_tag;
  using difference_type   = std::ptrdiff_t;
  using reference         = minibson::UNodeValue &;
  using pointer           = minibson::UNodeValue *;
};

template <>
struct iterator_traits<minibson::Document::Iterator> {
  using iterator_category = std::bidirectional_iterator_tag;
  using difference_type   = std::ptrdiff_t;
  using reference         = minibson::UNodeValue &;
  using pointer           = minibson::UNodeValue *;
};

template <>
struct iterator_traits<minibson::Array::ConstIterator> {
  using iterator_category = std::bidirectional_iterator_tag;
  using difference_type   = std::ptrdiff_t;
  using reference         = minibson::UNodeValue &;
  using pointer           = minibson::UNodeValue *;
};

template <>
struct iterator_traits<minibson::Array::Iterator> {
  using iterator_category = std::bidirectional_iterator_tag;
  using difference_type   = std::ptrdiff_t;
  using reference         = minibson::UNodeValue &;
  using pointer           = minibson::UNodeValue *;
};
} // namespace std
