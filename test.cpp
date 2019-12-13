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

int main() {
  // create document for testing
  minibson::Document d;

  d.set("int32", 1);
  d.set("int64", 140737488355328LL);
  d.set("float", 30.20);
  d.set("string", std::string{"text"});
  d.set("binary", minibson::Binary(&SOME_BUF_STR, sizeof(SOME_BUF_STR)));
  d.set("boolean", true);
  d.set("document", minibson::Document().set("a", 3).set("b", 4));
  d.set("some_other_string", std::string{"some_other_text"});
  d.set("null");
  d.set("array", minibson::Array{}.push_back(0).push_back(1));

  d.set("some_value_for_change", 10);
  assert(d.get<int>("some_value_for_change") == 10);
  d.set("some_value_for_change", std::string("some_string"));
  assert(d.get<std::string>("some_value_for_change") == "some_string");
  d.erase("some_value_for_change");

  int   length = d.getSerializedSize();
  char *buffer = new char[length];
  d.serialize(buffer, length);

  // test microbson
  microbson::Document doc{buffer, length};

  assert(!doc.empty());
  assert(doc.valid(length));
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

  microbson::Document nestedDoc = doc.get<microbson::Document>("document");
  assert(nestedDoc.size() == 2);
  assert(nestedDoc.get<int32_t>("a") == 3);
  assert(nestedDoc.get<int32_t>("b") == 4);

  microbson::Array a = doc.get<microbson::Array>("array");
  assert(a.size() == 2);
  assert(a.at<int32_t>(0) == 0);
  assert(a.at<int32_t>(1) == 1);
  CHECK_EXCEPT(a.at<int>(2), bson::OutOfRange);

  microbson::Binary binary = doc.get<microbson::Binary>("binary");
  assert(binary.first != nullptr);
  assert(binary.second == sizeof(SOME_BUF_STR));

  assert((reinterpret_cast<const char *>(binary.first)) ==
         std::string_view(SOME_BUF_STR));

  delete[] buffer;

  return EXIT_SUCCESS;
}
