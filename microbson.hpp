// microbson.hpp

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#define SIZE_OF_BSON_TYPE 1
#define SIZE_OF_ZERO_BYTE 1
#define SIZE_OF_BSON_SIZE 4
#define SIZE_OF_BSON_SUBTYPE 1

#define SIZE_OF_BOOLEAN_VALUE 1
#define SIZE_OF_INT32_VALUE 4
#define SIZE_OF_INT64_VALUE 8
#define SIZE_OF_DOUBLE_VALUE 8
#define SIZE_OF_NULL_VALUE 0

#define MINIMAL_SIZE_OF_BSON_DOCUMENT SIZE_OF_BSON_SIZE + SIZE_OF_ZERO_BYTE

#define MINIMAL_SIZE_OF_BSON_NODE SIZE_OF_BSON_TYPE + 1 + SIZE_OF_ZERO_BYTE
#define MINIMAL_SIZE_OF_BSON_NULL_NODE 3
#define MINIMAL_SIZE_OF_BSON_INT32_NODE                                        \
  MINIMAL_SIZE_OF_BSON_NODE + SIZE_OF_INT32_VALUE
#define MINIMAL_SIZE_OF_BSON_INT64_NODE                                        \
  MINIMAL_SIZE_OF_BSON_NODE + SIZE_OF_INT64_VALUE
#define MINIMAL_SIZE_OF_BSON_DOUBLE_NODE                                       \
  MINIMAL_SIZE_OF_BSON_NODE + SIZE_OF_DOUBLE_VALUE
#define MINIMAL_SIZE_OF_BSON_BOOLEAN_NODE                                      \
  MINIMAL_SIZE_OF_BSON_NODE + SIZE_OF_BOOLEAN_VALUE
#define MINIMAL_SIZE_OF_BSON_STRING_NODE                                       \
  MINIMAL_SIZE_OF_BSON_NODE + 1 + SIZE_OF_ZERO_BYTE
#define MINIMAL_SIZE_OF_BSON_BINARY_NODE                                       \
  MINIMAL_SIZE_OF_BSON_NODE + SIZE_OF_BSON_SIZE + SIZE_OF_ZERO_BYTE
#define MINIMAL_SIZE_OF_BSON_DOCUMENT_NODE                                     \
  MINIMAL_SIZE_OF_BSON_NODE + MINIMAL_SIZE_OF_BSON_DOCUMENT

namespace bson {
class Exception {
public:
  virtual ~Exception()                      = default;
  virtual const char *what() const noexcept = 0;
};

class BadCast final
    : virtual public Exception
    , virtual public std::bad_cast {
public:
  using std::bad_cast::bad_cast;

  [[nodiscard]] const char *what() const noexcept override {
    return std::bad_cast::what();
  }
};

class InvalidArgument final
    : virtual public Exception
    , virtual public std::invalid_argument {
public:
  using std::invalid_argument::invalid_argument;

  [[nodiscard]] const char *what() const noexcept override {
    return std::invalid_argument::what();
  }
};

class OutOfRange final
    : virtual public Exception
    , virtual public std::out_of_range {
public:
  using std::out_of_range::out_of_range;

  [[nodiscard]] const char *what() const noexcept override {
    return std::out_of_range::what();
  }
};

enum NodeType {
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

enum Scalar {}; // special value for scalars

// needed for prevent warning about enum compare
[[nodiscard]] constexpr bool operator==(NodeType lhs, int rhs) noexcept {
  return int(lhs) == rhs;
}
[[nodiscard]] constexpr bool operator!=(NodeType lhs, int rhs) noexcept {
  return int(lhs) != rhs;
}
} // namespace bson

namespace microbson {
/**\brief must contains:
 * - enum {node_type_code = ...} (required)
 * - using return_type = ... (required)
 * - static return_type converter(const void *ptr) (required)
 */
template <class T>
struct type_traits {};

using byte   = uint8_t;
using Binary = std::pair<const void *, int32_t>;

class Node {
public:
  explicit Node(const byte *data)
      : data_{data} {}

  [[nodiscard]] constexpr bson::NodeType type() const noexcept {
    return static_cast<bson::NodeType>(*data_);
  }

  [[nodiscard]] inline std::string_view key() const noexcept {
    return std::string_view{
        reinterpret_cast<const char *>(data_ + SIZE_OF_ZERO_BYTE)};
  }

  /**\return binary length of the node. In case of some unexpected value return
   * 0
   */
  [[nodiscard]] int length() const noexcept {
    int result = SIZE_OF_BSON_TYPE + this->key().size() + SIZE_OF_ZERO_BYTE;

    if (result ==
        SIZE_OF_BSON_TYPE + SIZE_OF_ZERO_BYTE) { // then we have invalid key
      return 0;
    }

    switch (this->type()) {
    case bson::double_node:
      result += SIZE_OF_DOUBLE_VALUE;
      break;
    case bson::document_node:
    case bson::array_node:
      result += *reinterpret_cast<const int32_t *>(data_ + result);
      break;
    case bson::string_node:
      result += (SIZE_OF_BSON_SIZE +
                 *reinterpret_cast<const int32_t *>(data_ + result));
      break;
    case bson::binary_node:
      result += (SIZE_OF_BSON_SIZE +
                 *reinterpret_cast<const int32_t *>(data_ + result) +
                 SIZE_OF_ZERO_BYTE);
      break;
    case bson::boolean_node:
      result += SIZE_OF_BOOLEAN_VALUE;
      break;
    case bson::null_node:
      result += SIZE_OF_NULL_VALUE;
      break;
    case bson::int32_node:
      result += SIZE_OF_INT32_VALUE;
      break;
    case bson::int64_node:
      result += SIZE_OF_INT64_VALUE;
      break;
    default:
      result = 0;
      break;
    };

    return result;
  }

  /**\return value in current node
   * \throw bson::BadCast if can not convert
   */
  template <class InputType>
  typename type_traits<InputType>::return_type value() const noexcept(false) {
    using return_type = typename type_traits<InputType>::return_type;

    constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;
    constexpr return_type (*converter)(const void *) =
        type_traits<InputType>::converter;

    if (this->type() != nodeTypeCode) {
      throw bson::BadCast{};
    }

    const byte *offset =
        data_ + SIZE_OF_BSON_TYPE + this->key().size() + SIZE_OF_ZERO_BYTE;

    return converter(offset);
  }

  [[nodiscard]] const void *data() const noexcept { return data_; }

  template <class InputType>
  [[nodiscard]] bool valid(int maxLength) const noexcept {
    constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

    if (this->type() != nodeTypeCode) {
      return false;
    }

    switch (this->type()) {
    case bson::boolean_node:
      if (maxLength < MINIMAL_SIZE_OF_BSON_BOOLEAN_NODE) {
        return false;
      }
      break;
    case bson::double_node:
      if (maxLength < MINIMAL_SIZE_OF_BSON_DOUBLE_NODE) {
        return false;
      }
      break;
    case bson::int32_node:
      if (maxLength < MINIMAL_SIZE_OF_BSON_INT32_NODE) {
        return false;
      }
      break;
    case bson::int64_node:
      if (maxLength < MINIMAL_SIZE_OF_BSON_INT64_NODE) {
        return false;
      }
      break;
    case bson::string_node:
      if (maxLength < MINIMAL_SIZE_OF_BSON_STRING_NODE) {
        return false;
      }
      break;
    case bson::binary_node:
      if (maxLength < MINIMAL_SIZE_OF_BSON_BINARY_NODE) {
        return false;
      }
      break;
    case bson::document_node:
    case bson::array_node:
      if (maxLength < MINIMAL_SIZE_OF_BSON_DOCUMENT_NODE) {
        return false;
      }
      break;
    case bson::null_node:
      if (maxLength < MINIMAL_SIZE_OF_BSON_NULL_NODE) {
        return false;
      }
      break;
    default:
      return false;
      break;
    }

    if (int length = this->length(); !length || length > maxLength) {
      return false;
    }

    return true;
  }

private:
  const byte *data_;
};

class Document {
public:
  Document() noexcept
      : data_{nullptr}
      , bufferLength_{0} {}

  virtual ~Document() = default;

  /**\param data pointer to serialized bson data
   * \param length size of buffer with bson data. Must be equal or greater then
   * serialized size of bson
   * \warning in konstructor it don't check input buffer on cantaining valid
   * bson. For validate it you need use special method, @see valid
   */
  Document(const void *data, int length) noexcept
      : data_{reinterpret_cast<const byte *>(data)}
      , bufferLength_{length} {}

  [[nodiscard]] inline const void *data() const { return data_; }

  [[nodiscard]] inline virtual bson::NodeType type() const noexcept {
    return bson::NodeType::document_node;
  }

  /**\brief check all nested fields of bson and return true if all fine,
   * otherwise - false
   */
  [[nodiscard]] bool valid() const noexcept;

  [[nodiscard]] inline bool empty() const noexcept { return !data_; }

  /**\return binary length of the document
   */
  [[nodiscard]] inline int length() const noexcept {
    if (data_) {
      return *reinterpret_cast<const int *>(data_);
    } else {
      return 0;
    }
  }

  /**\return capacity of nodes in the document
   */
  [[nodiscard]] inline int size() const noexcept;

  /**\brief forward iterator
   */
  class ConstIterator final {
    friend Document;

  public:
    ConstIterator()
        : offset_{nullptr} {}

    ConstIterator &operator++() noexcept {
      offset_ += Node{offset_}.length(); // XXX because length() can be 0 in
                                         // case of some not valid bson, this
                                         // operation can have unexpected result
      return *this;
    }

    [[nodiscard]] bool operator==(const ConstIterator &rhs) const noexcept {
      return this->offset_ == rhs.offset_;
    }

    [[nodiscard]] bool operator!=(const ConstIterator &rhs) const noexcept {
      return this->offset_ != rhs.offset_;
    }

    [[nodiscard]] Node operator*() noexcept { return Node{offset_}; }

    [[nodiscard]] bson::NodeType type() const noexcept {
      return Node{offset_}.type();
    }
    [[nodiscard]] std::string_view key() const noexcept {
      return Node{offset_}.key();
    }

    template <class InputType>
    typename type_traits<InputType>::return_type value() const noexcept(false) {
      return Node{offset_}.value<InputType>();
    }

  private:
    /**\param toEnd move iterator to end of current document
     */
    ConstIterator(const byte *offset)
        : offset_{offset} {}

  private:
    const byte *offset_;
  };

  [[nodiscard]] inline ConstIterator begin() const noexcept {
    if (this->empty()) {
      return ConstIterator{};
    }

    return ConstIterator{data_ + SIZE_OF_BSON_SIZE};
  }

  [[nodiscard]] inline ConstIterator end() const noexcept {
    if (this->empty()) {
      return ConstIterator{};
    }

    return ConstIterator{data_ + this->length() -
                         1 /*because at the end we have `\0`*/};
  }

  template <class InputType>
  [[nodiscard]] bool contains(std::string_view key) const noexcept;

  /**\brief same as template function contains, but not check type
   */
  [[nodiscard]] bool contains(std::string_view key) const noexcept;

  /**\throw bson::OutOfRange if value not found or if value have
   * different type
   * \brief this safely function for get value from bson
   * array by index, BUT!: this is very slowly. Use iterator where ever
   * it possible
   */
  template <class InputType>
  typename type_traits<InputType>::return_type get(std::string_view key) const
      noexcept(false);

private:
  const byte *data_;
  int         bufferLength_;
};

class Array final : public Document {
public:
  using Document::Document;

  /**\throw bson::OutOfRange if no value by index `i`
   * \brief this safely function for get value from bson array by index,
   * BUT!: this is very slowly. Use iterator where ever it possible
   */
  template <class InputType>
  typename type_traits<InputType>::return_type at(int i) const noexcept(false);

  template <class T>
  T get(std::string_view) const = delete;

  template <class T>
  bool contains(std::string_view) const noexcept = delete;
  bool contains(std::string_view) const noexcept = delete;

  template <class T>
  [[nodiscard]] bool contains(int i) const noexcept {
    return Document::contains<T>(std::to_string(i));
  }

  [[nodiscard]] inline bson::NodeType type() const noexcept override {
    return bson::array_node;
  }
};

template <>
struct type_traits<double> {
  enum { node_type_code = bson::double_node };
  using value_type  = double;
  using return_type = double;
  static double converter(const void *ptr) {
    return *reinterpret_cast<const double *>(ptr);
  }
};

template <>
struct type_traits<float> {
  enum { node_type_code = bson::double_node };
  using value_type  = double;
  using return_type = float;
  static double converter(const void *ptr) {
    return *reinterpret_cast<const double *>(ptr);
  }
};

template <>
struct type_traits<int32_t> {
  enum { node_type_code = bson::int32_node };
  using value_type  = int32_t;
  using return_type = int32_t;
  static int32_t converter(const void *ptr) {
    return *reinterpret_cast<const int32_t *>(ptr);
  }
};

template <>
struct type_traits<int64_t> {
  enum { node_type_code = bson::int64_node };
  using value_type  = int64_t;
  using return_type = int64_t;
  static int64_t converter(const void *ptr) {
    return *reinterpret_cast<const int64_t *>(ptr);
  }
};

template <>
struct type_traits<std::string> {
  enum { node_type_code = bson::string_node };
  using value_type  = std::string_view;
  using return_type = std::string;
  static std::string converter(const void *ptr) {
    return reinterpret_cast<const char *>(ptr) + SIZE_OF_BSON_SIZE;
  }
};

template <>
struct type_traits<std::string_view> {
  enum { node_type_code = bson::string_node };
  using value_type  = std::string_view;
  using return_type = std::string_view;
  static std::string_view converter(const void *ptr) {
    return reinterpret_cast<const char *>(ptr) + SIZE_OF_BSON_SIZE;
  }
};

template <>
struct type_traits<bool> {
  enum { node_type_code = bson::boolean_node };
  using value_type  = bool;
  using return_type = bool;
  static bool converter(const void *ptr) {
    return *reinterpret_cast<const byte *>(ptr);
  }
};

template <>
struct type_traits<void> {
  enum { node_type_code = bson::null_node };
  using value_type  = void;
  using return_type = void;
  static void converter(const void *) { return; }
};

template <>
struct type_traits<Array> {
  enum { node_type_code = bson::array_node };
  using value_type  = Array;
  using return_type = Array;
  static Array converter(const void *ptr) {
    return Array{ptr, *reinterpret_cast<const int *>(ptr)};
  }
};

template <>
struct type_traits<Document> {
  enum { node_type_code = bson::document_node };
  using value_type  = Document;
  using return_type = Document;
  static Document converter(const void *ptr) {
    return Document{ptr, *reinterpret_cast<const int *>(ptr)};
  }
};

template <>
struct type_traits<Binary> {
  enum { node_type_code = bson::binary_node };
  using value_type  = Binary;
  using return_type = Binary;
  static Binary converter(const void *ptr) {
    return Binary{reinterpret_cast<const byte *>(ptr) + SIZE_OF_BSON_SIZE +
                      SIZE_OF_BSON_SUBTYPE,
                  *reinterpret_cast<const int *>(ptr)};
  }
};

/**\brief Special Case for get some scalar (int32, int64 or double) as double
 */
template <>
struct type_traits<bson::Scalar> {
  using value_type  = bson::Scalar;
  using return_type = double;
};
} // namespace microbson

namespace std {
template <>
struct iterator_traits<microbson::Document::ConstIterator> {
  using iterator_category = std::forward_iterator_tag;
  using difference_type   = std::ptrdiff_t;
};
} // namespace std

namespace microbson {
inline int Document::size() const noexcept {
  return std::distance(this->begin(), this->end());
}

inline bool Document::valid() const noexcept {
  // first of all check by bufferLength
  if (data_ &&
      !(bufferLength_ >= MINIMAL_SIZE_OF_BSON_DOCUMENT &&
        this->length() <= bufferLength_ && data_[this->length() - 1] == '\0')) {
    return false;
  }

  auto end = this->end();
  for (auto i = this->begin(); i != end; ++i) {
    Node node      = *i;
    int  maxLength = std::distance(i.offset_, end.offset_);
    switch (node.type()) {
    case bson::string_node:
      if (!node.valid<std::string_view>(maxLength)) {
        return false;
      }
      break;
    case bson::boolean_node:
      if (!node.valid<bool>(maxLength)) {
        return false;
      }
      break;
    case bson::int32_node:
      if (!node.valid<int32_t>(maxLength)) {
        return false;
      }
      break;
    case bson::int64_node:
      if (!node.valid<int64_t>(maxLength)) {
        return false;
      }
      break;
    case bson::double_node:
      if (!node.valid<double>(maxLength)) {
        return false;
      }
      break;
    case bson::null_node:
      if (!node.valid<void>(maxLength)) {
        return false;
      }
      break;
    case bson::binary_node:
      if (!node.valid<Binary>(maxLength)) {
        return false;
      }
      break;
    case bson::array_node:
      if (!node.valid<Array>(maxLength) || !node.value<Array>().valid()) {
        return false;
      }
      break;
    case bson::document_node:
      if (!node.valid<Document>(maxLength) || !node.value<Document>().valid()) {
        return false;
      }
      break;
    default:
      return false;
    }
  }

  return true;
}

template <class InputType>
inline bool Document::contains(std::string_view key) const noexcept {
  constexpr int nodeTypeCode = type_traits<InputType>::node_type_code;

  if (auto found = std::find_if(
          this->begin(),
          this->end(),
          [key](Node node) {
            if (node.key() ==
                key) { // we not need check here, because in bson can not
                       // contains two or more values with same key
              return true;
            }

            return false;
          });
      found != this->end()) {
    if ((*found).type() == nodeTypeCode) {
      return true; // only with same key and type
    }
  }

  return false;
}

inline bool Document::contains(std::string_view key) const noexcept {
  if (auto found = std::find_if(this->begin(),
                                this->end(),
                                [key](Node node) {
                                  if (node.key() == key) {
                                    return true;
                                  }

                                  return false;
                                });
      found != this->end()) {
    return true;
  }

  return false;
}

template <class InputType>
inline typename type_traits<InputType>::return_type
Document::get(std::string_view key) const {
  if (auto found = std::find_if(this->begin(),
                                this->end(),
                                [key](Node node) {
                                  if (node.key() == key) {
                                    return true;
                                  }

                                  return false;
                                });
      found != this->end()) {
    return (*found).template value<InputType>();
  } else {
    throw bson::OutOfRange{"no value by key: " + std::string{key}};
  }
}

template <class InputType>
inline typename type_traits<InputType>::return_type Array::at(int i) const {
  auto iter = this->begin();
  for (int counter = 0; iter != this->end() && counter < i; ++iter, ++counter)
    ;
  if (iter != this->end()) {
    return (*iter).template value<InputType>();
  } else {
    throw bson::OutOfRange{"no value by index: " + std::to_string(i)};
  }
}

/**\brief special case if we need get some number and we don't care about type
 * of it
 */
template <>
inline typename type_traits<bson::Scalar>::return_type
Node::value<bson::Scalar>() const noexcept(false) {
  const byte *offset =
      data_ + SIZE_OF_BSON_TYPE + this->key().size() + SIZE_OF_ZERO_BYTE;

  switch (this->type()) {
  case bson::double_node:
    return *reinterpret_cast<const double *>(offset);
  case bson::int32_node:
    return *reinterpret_cast<const int32_t *>(offset);
  case bson::int64_node:
    return *reinterpret_cast<const int64_t *>(offset);
  default:
    throw bson::BadCast{};
  }
}

template <>
inline bool Document::contains<bson::Scalar>(std::string_view key) const
    noexcept {
  if (auto found = std::find_if(
          this->begin(),
          this->end(),
          [key](Node node) {
            if (node.key() ==
                key) { // we not need check here, because in bson can not
                       // contains two or more values with same key
              return true;
            }

            return false;
          });
      found != this->end()) {
    if (auto type = (*found).type(); type == bson::double_node ||
                                     type == bson::int32_node ||
                                     type == bson::int64_node) {
      return true;
    }
  }

  return false;
}
} // namespace microbson
