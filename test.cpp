// test.cpp

#include "microbson.hpp"
#include "minibson.hpp"
#include <cassert>
#include <iostream>

#define SOME_BUF_STR "some buf str"

#define CHECK_EXCEPT(x, type)                                                  \
  {                                                                            \
    try {                                                                      \
      x;                                                                       \
      assert(false);                                                           \
    } catch (type &) {                                                         \
    }                                                                          \
  }

// type placeholder for custom type_traits for microbson
struct String {};

namespace microbson {
template <>
struct type_traits<String> {
  enum { node_type_code = bson::binary_node };
  using return_type = std::string_view;
  static std::string_view converter(const void *ptr) {
    return std::string_view{reinterpret_cast<const char *>(ptr) + 5};
  }
};
} // namespace microbson

namespace minibson {
template <>
struct type_traits<String> {
  enum { node_type_code = bson::binary_node };
  using value_type  = Binary;
  using return_type = std::string_view;
  static std::string_view converter(const Binary &b) {
    return std::string_view{reinterpret_cast<const char *>(b.buf_.data())};
  }
  static Binary back_converter(const std::string_view &s) {
    Binary b;
    b.buf_.reserve(s.size() + 1 /*\0*/);
    std::copy(s.begin(), s.end(), std::back_inserter(b.buf_));
    b.buf_.push_back('\0');
    return b;
  }
};
} // namespace minibson

void minibson_test();
void microbson_test();

int main() {
  minibson_test();
  microbson_test();

  return EXIT_SUCCESS;
}

void minibson_test() {
  minibson::Document d;

  d.set("int32", 1);
  d.set("int64", 140737488355328LL);
  d.set("float", 30.20);
  d.set("string", std::string{"text"});
  d.set("string_view", std::string_view{"text"});
  d.set("cstring", "text");
  d.set("binary", minibson::Binary(&SOME_BUF_STR, sizeof(SOME_BUF_STR)));
  d.set("boolean", true);
  d.set("document", std::move(minibson::Document().set("a", 3).set("b", 4)));
  d.set("some_other_string", "some_other_text");
  d.set("null");
  d.set("array",
        std::move(minibson::Array{}
                      .push_back(0)
                      .push_back(1)
                      .push_back(std::string{"string"})
                      .push_back(std::string_view{"string_view"})
                      .push_back("cstring")));

  d.set("some_value_for_change", 10);
  assert(d.get<int32_t>("some_value_for_change") == 10);
  d.set("some_value_for_change", std::string("some_string"));
  assert(d.get<std::string>("some_value_for_change") == "some_string");
  d.erase("some_value_for_change");

  d.set<String>("custom", "custom");
  d.get<minibson::Binary>("custom");
  assert(d.get<String>("custom") == "custom");
  d.erase("custom");

  const minibson::Document &dConst = d;

  static_assert(
      std::is_reference<decltype(d.get<std::string>("string"))>::value);
  static_assert(
      !std::is_reference<decltype(d.get<std::string_view>("string"))>::value);
  static_assert(std::is_reference<decltype(d.get<double>("float"))>::value);
  static_assert(!std::is_reference<decltype(d.get<float>("float"))>::value);
  static_assert(
      std::is_reference<decltype(d.get<minibson::Array>("array"))>::value);

  static_assert(
      std::is_reference<decltype(dConst.get<std::string>("string"))>::value);
  static_assert(!std::is_reference<decltype(
                    dConst.get<std::string_view>("string"))>::value);
  static_assert(
      !std::is_reference<decltype(dConst.get<double>("float"))>::value);
  static_assert(
      !std::is_reference<decltype(dConst.get<float>("float"))>::value);
  static_assert(
      std::is_reference<decltype(dConst.get<minibson::Array>("array"))>::value);

  assert(d.contains("int32"));
  assert(d.contains("int64"));
  assert(d.contains("float"));
  assert(d.contains("boolean"));
  assert(d.contains("string"));
  assert(d.contains("string_view"));
  assert(d.contains("cstring"));

  assert(d.contains<int32_t>("int32"));
  assert(d.contains<int64_t>("int64"));
  assert(d.contains<double>("float"));
  assert(d.contains<bool>("boolean"));
  assert(d.contains<std::string_view>("string"));
  assert(d.contains<std::string_view>("string_view"));
  assert(d.contains<std::string_view>("cstring"));

  assert(d.contains<String>("binary"));

  assert(d.contains<bson::Scalar>("int32"));
  assert(d.contains<bson::Scalar>("int64"));
  assert(d.contains<bson::Scalar>("float"));

  assert(d.get<int32_t>("int32") == 1);
  assert(d.get<int64_t>("int64") == 140737488355328LL);
  assert(d.get<double>("float") == 30.20);
  assert(d.get<bool>("boolean") == true);
  assert(d.get<std::string_view>("string") == "text");
  assert(d.get<std::string_view>("string_view") == "text");
  assert(d.get<std::string_view>("cstring") == "text");
  assert(std::strcmp(d.get<const char *>("string"), "text") == 0);

  static_assert(
      std::is_same<decltype(d.get<bson::Scalar>("int32")), double>::value);
  static_assert(
      std::is_same<decltype(d.get<bson::Scalar>("int64")), double>::value);
  static_assert(
      std::is_same<decltype(d.get<bson::Scalar>("float")), double>::value);

  assert(d.get<bson::Scalar>("int32") == 1);
  assert(d.get<bson::Scalar>("int64") == 140737488355328LL);
  assert(d.get<bson::Scalar>("float") == 30.20);

  assert(d.get<String>("binary") == SOME_BUF_STR);

  minibson::Array arr;
  arr.push_back(int32_t{10});
  arr.push_back(int64_t{10});
  arr.push_back(double{10.0});
  arr.push_back(true);
  arr.push_back("text");
  arr.push_back(std::string{"text"});
  arr.push_back(std::string_view{"text"});
  arr.push_back();
  arr.push_back(minibson::Binary{SOME_BUF_STR, sizeof(SOME_BUF_STR)});
  arr.push_back<String>("custom");

  assert(arr.size() == 10);

  auto iter = arr.begin();
  assert((iter).type() == bson::int32_node);
  assert((++iter).type() == bson::int64_node);
  assert((++iter).type() == bson::double_node);
  assert((++iter).type() == bson::boolean_node);
  assert((++iter).type() == bson::string_node);
  assert((++iter).type() == bson::string_node);
  assert((++iter).type() == bson::string_node);
  assert((++iter).type() == bson::null_node);
  assert((++iter).type() == bson::binary_node);
  assert((++iter).type() == bson::binary_node);

  assert(++iter == arr.end());

  assert(arr.contains<int32_t>(0));
  assert(arr.contains<int64_t>(1));
  assert(arr.contains<double>(2));
  assert(arr.contains<bool>(3));
  assert(arr.contains<std::string_view>(4));
  assert(arr.contains<std::string_view>(5));
  assert(arr.contains<std::string_view>(6));
  assert(arr.contains<void>(7));
  assert(arr.contains<String>(8));
  assert(arr.contains<String>(9));

  assert(arr.at<int32_t>(0) == 10);
  assert(arr.at<int64_t>(1) == 10);
  assert(arr.at<double>(2) == 10.0);
  assert(arr.at<bool>(3) == true);
  assert(arr.at<std::string_view>(4) == "text");
  assert(arr.at<std::string_view>(5) == "text");
  assert(arr.at<std::string_view>(6) == "text");
  assert(std::is_void<decltype(arr.at<void>(7))>::value);
  assert(arr.at<String>(8) == SOME_BUF_STR);
  assert(arr.at<String>(9) == "custom");

  static_assert(std::is_same<decltype(arr.at<bson::Scalar>(0)), double>::value);
  static_assert(std::is_same<decltype(arr.at<bson::Scalar>(1)), double>::value);
  static_assert(std::is_same<decltype(arr.at<bson::Scalar>(2)), double>::value);

  assert(arr.at<bson::Scalar>(0) == 10);
  assert(arr.at<bson::Scalar>(1) == 10);
  assert(arr.at<bson::Scalar>(2) == 10.);

  const auto &arrConst = arr;

  static_assert(std::is_reference<decltype(arr.at<int32_t>(0))>::value);
  static_assert(std::is_reference<decltype(arr.at<int64_t>(1))>::value);
  static_assert(std::is_reference<decltype(arr.at<double>(2))>::value);
  static_assert(std::is_reference<decltype(arr.at<bool>(3))>::value);
  static_assert(std::is_reference<decltype(arr.at<std::string>(4))>::value);
  static_assert(
      !std::is_reference<decltype(arr.at<std::string_view>(5))>::value);
  static_assert(!std::is_reference<decltype(arr.at<const char *>(6))>::value);
  static_assert(
      std::is_reference<decltype(arr.at<minibson::Binary>(8))>::value);
  static_assert(!std::is_reference<decltype(arr.at<String>(9))>::value);

  static_assert(!std::is_reference<decltype(arrConst.at<int32_t>(0))>::value);
  static_assert(!std::is_reference<decltype(arrConst.at<int64_t>(1))>::value);
  static_assert(!std::is_reference<decltype(arrConst.at<double>(2))>::value);
  static_assert(!std::is_reference<decltype(arrConst.at<bool>(3))>::value);
  static_assert(std::is_reference<decltype(arr.at<std::string>(4))>::value);
  static_assert(
      !std::is_reference<decltype(arr.at<std::string_view>(5))>::value);
  static_assert(!std::is_reference<decltype(arr.at<const char *>(6))>::value);
  static_assert(
      std::is_reference<decltype(arrConst.at<minibson::Binary>(8))>::value);
  static_assert(!std::is_reference<decltype(arrConst.at<String>(9))>::value);

  // check copys and moving
  std::string alpha = "alpha";
  d.set("tmp", alpha);
  assert(alpha == "alpha");
  d.set("tmp", std::move(alpha));
  assert(alpha != "alpha");

  alpha = "alpha";
  arr.push_back(alpha);
  assert(alpha == "alpha");
  arr.push_back(std::move(alpha));
  assert(alpha != "alpha");
}

void microbson_test() {
  // create document for testing
  minibson::Document d;

  d.set("int32", 1);
  d.set("int64", 140737488355328LL);
  d.set("float", 30.20);
  d.set("string", std::string{"text"});
  d.set("binary", minibson::Binary(&SOME_BUF_STR, sizeof(SOME_BUF_STR)));
  d.set("boolean", true);
  d.set("document", std::move(minibson::Document().set("a", 3).set("b", 4)));
  d.set("some_other_string", "some_other_text");
  d.set("null");
  d.set("array",
        std::move(minibson::Array{}
                      .push_back(0)
                      .push_back<double>(1)
                      .push_back<int64_t>(2)
                      .push_back(std::string{"string"})));

  // serialize
  int   length = d.getSerializedSize();
  char *buffer = new char[length];
  d.serialize(buffer, length);

  CHECK_EXCEPT((microbson::Document{buffer, length - 1}),
               bson::InvalidArgument);
  microbson::Document{buffer,
                      length + 1}; // all right, because length of buffer is
                                   // bigger then length of serialized bson

  // test microbson
  microbson::Document doc{buffer, length};

  assert(!doc.empty());
  assert(doc.valid());
  assert(doc.size() == 10);

  assert(doc.contains("int32"));
  assert(doc.contains("int64"));
  assert(doc.contains("float"));
  assert(doc.contains("string"));
  assert(doc.contains("boolean"));
  assert(doc.contains("document"));
  assert(doc.contains("null"));
  assert(doc.contains("array"));
  assert(doc.contains("some_other_string"));
  assert(doc.contains("binary"));

  assert(doc.contains<int32_t>("int32"));
  assert(doc.contains<int64_t>("int64"));
  assert(doc.contains<double>("float"));
  assert(doc.contains<float>("float"));
  assert(doc.contains<std::string_view>("string"));
  assert(doc.contains<bool>("boolean"));
  assert(doc.contains<microbson::Document>("document"));
  assert(doc.contains<void>("null"));
  assert(doc.contains<microbson::Array>("array"));
  assert(doc.contains<microbson::Binary>("binary"));
  assert(doc.contains<int>("not exists") == false);

  assert(doc.contains<String>("binary"));

  assert(doc.contains<bson::Scalar>("int32"));
  assert(doc.contains<bson::Scalar>("int64"));
  assert(doc.contains<bson::Scalar>("float"));

  assert(doc.get<int32_t>("int32") == 1);
  assert(doc.get<int64_t>("int64") == 140737488355328LL);
  assert(doc.get<double>("float") ==
         30.20); // I don't care about comparison floating point numbers,
                 // becuase it is not calculated
  assert(doc.get<std::string_view>("string") == "text");
  assert(doc.get<bool>("boolean") == true);
  assert(std::is_void<decltype(doc.get<void>("null"))>::value);
  CHECK_EXCEPT(doc.get<int>("not exists"), bson::OutOfRange);
  CHECK_EXCEPT(doc.get<int>("string"), bson::BadCast);

  static_assert(
      std::is_same<decltype(doc.get<bson::Scalar>("int32")), double>::value);
  static_assert(
      std::is_same<decltype(doc.get<bson::Scalar>("int64")), double>::value);
  static_assert(
      std::is_same<decltype(doc.get<bson::Scalar>("float")), double>::value);

  assert(doc.get<bson::Scalar>("int32") == 1);
  assert(doc.get<bson::Scalar>("int64") == 140737488355328LL);
  assert(doc.get<bson::Scalar>("float") == 30.20);

  microbson::Document nestedDoc = doc.get<microbson::Document>("document");
  assert(nestedDoc.size() == 2);
  assert(nestedDoc.get<int32_t>("a") == 3);
  assert(nestedDoc.get<int32_t>("b") == 4);

  microbson::Array a = doc.get<microbson::Array>("array");
  assert(a.contains<int32_t>(0));
  assert(a.contains<double>(1));
  assert(a.contains<int64_t>(2));
  assert(a.contains<std::string_view>(3));

  assert(a.size() == 4);
  assert(a.at<int32_t>(0) == 0);
  assert(a.at<double>(1) == 1);
  assert(a.at<int64_t>(2) == 2);
  assert(a.at<std::string_view>(3) == "string");
  CHECK_EXCEPT(a.at<int>(3), bson::BadCast);
  CHECK_EXCEPT(a.at<int>(4), bson::OutOfRange);

  static_assert(std::is_same<decltype(a.at<bson::Scalar>(0)), double>::value);
  static_assert(std::is_same<decltype(a.at<bson::Scalar>(1)), double>::value);
  static_assert(std::is_same<decltype(a.at<bson::Scalar>(2)), double>::value);

  assert(a.at<bson::Scalar>(0) == 0);
  assert(a.at<bson::Scalar>(1) == 1);
  assert(a.at<bson::Scalar>(2) == 2);

  microbson::Binary binary = doc.get<microbson::Binary>("binary");
  assert(binary.first != nullptr);
  assert(binary.second == sizeof(SOME_BUF_STR));

  // new type
  std::string_view s = doc.get<String>("binary");
  assert(s == SOME_BUF_STR);

  assert((reinterpret_cast<const char *>(binary.first)) ==
         std::string_view(SOME_BUF_STR));

  minibson::Array &arr = d.get<minibson::Array>("array");
  auto             j   = a.begin();
  for (auto i = arr.begin(); i != arr.end() && j != a.end(); ++i, ++j) {
    switch (j.type()) {
    case bson::int32_node:
      assert(j.value<int32_t>() == i.value<int32_t>());
      break;
    case bson::string_node:
      assert(j.value<std::string_view>() == i.value<const char *>());
      break;
    default:
      break;
    }
  }

  auto iter = arr.begin();
  std::for_each(a.begin(), a.end(), [&iter](const microbson::Node &node) {
    switch (iter.type()) {
    case bson::int32_node:
      assert(iter.value<int32_t>() == node.value<int32_t>());
      break;
    case bson::string_node:
      assert(iter.value<std::string_view>() == node.value<std::string_view>());
      break;
    default:
      break;
    }

    ++iter;
  });

  delete[] buffer;

  // test invalid bson
  uint8_t invalidBson[10]{};
  *reinterpret_cast<int *>(invalidBson) = 10;
  *(invalidBson + 4)                    = 0x10;
  microbson::Document invalidDoc{invalidBson, 10};
  assert(!invalidDoc.valid());

  // test empty bson
  microbson::Document emptyDoc{};
  assert(emptyDoc.empty());
  assert(emptyDoc.size() == 0);
  assert(emptyDoc.length() == 0);
  assert(std::distance(emptyDoc.begin(), emptyDoc.end()) == 0);
}
