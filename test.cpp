#include "bson.hpp"
#include <cassert>

int main() {
    using namespace bson;
    using namespace std;

    document d;

    d.set("int32", 1);
    d.set("int64", 140737488355328LL);
    d.set("float", 30.20);
    d.set("string", "text");
    d.set("binary", binary::buffer(&d, sizeof(d)));
    d.set("boolean", true);
    d.set("document", document().set("a", 3).set("b", 4));
    d.set("null");
    
    assert(d.contains<int>("int32"));
    assert(d.contains<long long int>("int64"));
    assert(d.contains<double>("float"));
    assert(d.contains<std::string>("string"));
    assert(d.contains<binary::buffer>("binary"));
    assert(d.contains<bool>("boolean"));
    assert(d.contains<document>("document"));
    assert(d.contains("null"));

    assert(d.get("int32", 0) == 1);
    assert(d.get("int64", 0LL) == 140737488355328LL);
    assert(d.get("float", 0.0) == 30.20);
    assert(d.get("string", "") == "text");
    assert(d.get("binary", binary::buffer(NULL, 0)).length == sizeof(d));
    assert(d.get("boolean", false) == true);
    assert(d.get("document", document()).contains("a") && d.get("document", document()).contains("b"));

    size_t size = d.get_serialized_size();	
    char* buffer = new char[size];
    d.serialize(buffer, size);
    
    document d1(buffer, size);
    
    delete[] buffer;
    
    assert(d1.get("int32", 0) == 1);
    assert(d1.get("int64", 0LL) == 140737488355328LL);
    assert(d1.get("float", 0.0) == 30.20);
    assert(d1.get("string", "") == "text");
    assert(d1.get("binary", binary::buffer(NULL, 0)).length == sizeof(d));
    assert(d1.get("boolean", false) == true);
    assert(d1.get("document", document()).contains("a") && d.get("document", document()).contains("b"));
    assert(d1.contains("null"));

    return 0;
}
