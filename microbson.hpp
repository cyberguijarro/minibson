// microbson.hpp

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

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

  const char *what() const noexcept override { return std::bad_cast::what(); }
};

class InvalidArgument final
    : virtual public Exception
    , virtual public std::invalid_argument {
public:
  using std::invalid_argument::invalid_argument;

  const char *what() const noexcept override {
    return std::invalid_argument::what();
  }
};

class OutOfRange final
    : virtual public Exception
    , virtual public std::out_of_range {
public:
  using std::out_of_range::out_of_range;

  const char *what() const noexcept override {
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
constexpr bool operator==(NodeType lhs, int rhs) {
  return int(lhs) == rhs;
}
constexpr bool operator!=(NodeType lhs, int rhs) {
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

  constexpr bson::NodeType type() const noexcept {
    return static_cast<bson::NodeType>(*data_);
  }

  inline std::string_view key() const noexcept {
    return std::string_view{reinterpret_cast<const char *>(data_) + 1};
  }

  /**\return binary length of the node. In case of some unexpected value return
   * 0
   */
  int length() const noexcept {
    int result = 1 /*type*/ + this->key().size() + 1 /*\0*/;

    switch (this->type()) {
    case bson::double_node:
      result += sizeof(double);
      break;
    case bson::document_node:
    case bson::array_node:
      result += *reinterpret_cast<const int *>(data_ + result);
      break;
    case bson::string_node:
      result += (sizeof(int) + *reinterpret_cast<const int *>(data_ + result));
      break;
    case bson::binary_node:
      result += (sizeof(int) + *reinterpret_cast<const int *>(data_ + result) +
                 1 /*\0*/);
      break;
    case bson::boolean_node:
      result += 1;
    case bson::null_node:
      break;
    case bson::int32_node:
      result += sizeof(int32_t);
      break;
    case bson::int64_node:
      result += sizeof(int64_t);
      break;
    case bson::unknown_node:
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

    constexpr return_type (*converter)(const void *) =
        type_traits<InputType>::converter;

    if (this->type() != type_traits<InputType>::node_type_code) {
      throw bson::BadCast{};
    }

    const byte *offset = data_ + 1 /*type*/ + this->key().size() + 1 /*\0*/;

    return converter(offset);
  }

  const void *data() const noexcept { return data_; }

private:
  const byte *data_;
};

class Document {
public:
  Document()
      : data_{nullptr} {}

  /**\param data pointer to serialized bson data
   * \param length size of bson data. Not required, needed for validation
   * \throw bson::InvalidArgument if data isn't `nullptr`, size set and
   * validation failed
   */
  Document(const void *data, int length)
      : data_{reinterpret_cast<const byte *>(data)} {
    if (data_ && !this->valid(length)) {
      throw bson::InvalidArgument{
          "invalid bson; input length: " + std::to_string(length) +
          ", serialized length: " + std::to_string(this->length())};
    }
  }

  inline const void *data() const { return data_; }

  /**\brief valid bson document have to have size >= 7, encode own size in
   * first four bytes and last symbol have to be \0
   */
  inline bool valid(int length) const noexcept {
    return length >= 7 && this->length() == length && data_[length - 1] == '\0';
  }

  inline bool empty() const noexcept { return !data_; }

  /**\return binary length of the document
   */
  inline int length() const noexcept {
    if (data_) {
      return *reinterpret_cast<const int *>(data_);
    } else {
      return 0;
    }
  }

  /**\return capacity of nodes in the document
   */
  int size() const noexcept;

  /**\brief forward iterator
   */
  class ConstIterator final {
    friend Document;

  public:
    ConstIterator()
        : offset_{nullptr} {}

    ConstIterator &operator++() noexcept {
      offset_ += Node{offset_}.length();
      return *this;
    }

    bool operator==(const ConstIterator &rhs) const noexcept {
      return this->offset_ == rhs.offset_;
    }

    bool operator!=(const ConstIterator &rhs) const noexcept {
      return this->offset_ != rhs.offset_;
    }

    Node operator*() noexcept { return Node{offset_}; }

    bson::NodeType   type() const noexcept { return Node{offset_}.type(); }
    std::string_view key() const noexcept { return Node{offset_}.key(); }

    template <class InputType>
    typename type_traits<InputType>::return_type value() const {
      return Node{offset_}.value<InputType>();
    }

  private:
    /**\param toEnd move iterator to end of current document
     */
    ConstIterator(const byte *data, bool toEnd = false)
        : offset_{nullptr} {
      if (data) {
        if (toEnd) {
          offset_ = data + *reinterpret_cast<const int *>(data) -
                    1; // -1 because last element is \0
        } else {
          offset_ = data + sizeof(int); // move offset to first element
        }
      }
    }

  private:
    const byte *offset_;
  };

  ConstIterator begin() const noexcept { return ConstIterator{data_}; }

  ConstIterator end() const noexcept { return ConstIterator{data_, true}; }

  template <class InputType>
  bool contains(std::string_view key) const noexcept;

  /**\brief same as template function contains, but not check type
   */
  bool contains(std::string_view key) const noexcept;

  /**\throw bson::OutOfRange if value not found or if value have different type
   * \brief this safely function for get value from bson array by index, BUT!:
   * this is very slowly. Use iterator where ever it possible
   */
  template <class InputType>
  typename type_traits<InputType>::return_type get(std::string_view key) const
      noexcept(false);

private:
  const byte *data_;
};

class Array final : public Document {
public:
  using Document::Document;

  /**\throw bson::OutOfRange if no value by index `i`
   * \brief this safely function for get value from bson array by index, BUT!:
   * this is very slowly. Use iterator where ever it possible
   */
  template <class InputType>
  typename type_traits<InputType>::return_type at(int i) const noexcept(false);

  template <class T>
  bool contains(std::string_view) const noexcept = delete;

  template <class T>
  T get(std::string_view) const = delete;
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
struct type_traits<long long int> {
  enum { node_type_code = bson::int64_node };
  using value_type  = int64_t;
  using return_type = long long int;
  static long long int converter(const void *ptr) {
    return *reinterpret_cast<const long long int *>(ptr);
  }
};

template <>
struct type_traits<std::string> {
  enum { node_type_code = bson::string_node };
  using value_type  = std::string_view;
  using return_type = std::string;
  static std::string converter(const void *ptr) {
    return reinterpret_cast<const char *>(ptr) + 4 /*size*/;
  }
};

template <>
struct type_traits<std::string_view> {
  enum { node_type_code = bson::string_node };
  using value_type  = std::string_view;
  using return_type = std::string_view;
  static std::string_view converter(const void *ptr) {
    return reinterpret_cast<const char *>(ptr) + 4 /*size*/;
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
    return Binary{reinterpret_cast<const byte *>(ptr) + 4 /*size*/ +
                      1 /*subtype*/,
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

template <class InputType>
inline bool Document::contains(std::string_view key) const noexcept {
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
    if ((*found).type() == type_traits<InputType>::node_type_code) {
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
  const byte *offset = data_ + 1 /*type*/ + this->key().size() + 1 /*\0*/;

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
} // namespace microbson
