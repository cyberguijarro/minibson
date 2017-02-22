#pragma once

#include <cstring>
#include <cstdio>
#include <string>
#include <iostream>
#include <map>

namespace minibson {

    // Basic types

    enum node_type {
        double_node = 0x01,
        string_node = 0x02,
        document_node = 0x03,
        binary_node = 0x05,
        boolean_node = 0x08,
        null_node = 0x0A,
        int32_node = 0x10,
        int64_node = 0x12,
        unknown_node = 0xFF
    };
    
    template<typename T> struct type_converter { };
   
    class node {
        public:
            virtual ~node() { }
            virtual void serialize(void* const buffer, const size_t count) const = 0;
            virtual size_t get_serialized_size() const = 0;
            virtual unsigned char get_node_code() const { return 0; }
            virtual node* copy() const = 0;
            virtual void dump(std::ostream&) const = 0;
            virtual void dump(std::ostream& stream, int level) const { dump(stream); }
            static node* create(node_type type, const void* const buffer, const size_t count);
    };

    // Value types

    class null : public node {
        public:
            null() { }

            null(const void* const buffer, const size_t count) { }

            void serialize(void* const buffer, const size_t count) const { }

            size_t get_serialized_size() const { 
                return 0; 
            }

            unsigned char get_node_code() const {
                return null_node;
            }

            node* copy() const {
                return new null();
            }

            void dump(std::ostream& stream) const { stream << "null"; };
    };

    template<typename T, node_type N>
        class scalar : public node {
            private:
                T value;
            public:
                scalar(const T value) : value(value) { }

                scalar(const void* const buffer, const size_t count) {
                    value = *reinterpret_cast<const T*>(buffer);
                };

                void serialize(void* const buffer, const size_t count) const {
                    *reinterpret_cast<T*>(buffer) = value;
                }

                size_t get_serialized_size() const {
                    return sizeof(T);
                }

                unsigned char get_node_code() const {
                    return N;
                }

                node* copy() const {
                    return new scalar<T, N>(value);
                }

                void dump(std::ostream& stream) const { stream << value; };

                const T& get_value() const { return value; }
        };

    class int32 : public scalar<int, int32_node> {
        public:
            int32(const int value) : scalar<int, int32_node>(value) { }

            int32(const void* const buffer, const size_t count) : scalar<int, int32_node>(buffer, count) { };
    };
    
    template<> struct type_converter<int> { enum { node_type_code = int32_node }; typedef int32 node_class; };
    
    class int64 : public scalar<long long int, int64_node> {
        public:
            int64(const long long int value) : scalar<long long int, int64_node>(value) { }

            int64(const void* const buffer, const size_t count) : scalar<long long int, int64_node>(buffer, count) { };
    };
    
    template<> struct type_converter<long long int> { enum { node_type_code = int64_node }; typedef int64 node_class; };

    class Double : public scalar<double, double_node> {
        public:
            Double(const double value) : scalar<double, double_node>(value) { }

            Double(const void* const buffer, const size_t count) : scalar<double, double_node>(buffer, count) { };
    };
    
    template<> struct type_converter<double> { enum { node_type_code = double_node }; typedef Double node_class; };

    class string : public node {
        private:
            std::string value;
        public:
            string(const std::string& value) : value(value) { }

            string(const void* const buffer, const size_t count) {
                if ( count >= 5 ) {
                    const size_t max = count - sizeof(unsigned int);
                    const size_t actual = *reinterpret_cast<const unsigned int*>(
                        buffer
                    );

                    value.assign(
                        reinterpret_cast<const char*>(buffer) + sizeof(unsigned int),
                        std::min( actual, max ) - 1
                    );
                }
            };

            void serialize(void* const buffer, const size_t count) const {
                *reinterpret_cast<unsigned int*>(buffer) = value.length() + 1;
                std::memcpy(reinterpret_cast<char*>(buffer) + sizeof(unsigned int), value.c_str(), value.length());
                *(reinterpret_cast<char*>(buffer) + count - 1) = '\0';
            }

            size_t get_serialized_size() const {
                return sizeof(unsigned int) + value.length() + 1;
            }

            unsigned char get_node_code() const {
                return string_node;
            }

            node* copy() const {
                return new string(value);
            }

            void dump(std::ostream& stream) const { stream << "\"" << value << "\""; };
            
            const std::string& get_value() const { return value; }
    };
    
    template<> struct type_converter<std::string> { enum { node_type_code = string_node }; typedef string node_class; };

    class boolean : public node {
        private:
            bool value;
        public:
            boolean(const bool value) : value(value) { }

            boolean(const void* const buffer, const size_t count) {
                switch (*reinterpret_cast<const unsigned char*>(buffer)) {
                    case 1: value = true; break;
                    default: value = false; break;
                }
            };

            void serialize(void* const buffer, const size_t count) const {
                *reinterpret_cast<unsigned char*>(buffer) = value ? true : false;
            }

            size_t get_serialized_size() const {
                return 1;
            }

            unsigned char get_node_code() const {
                return boolean_node;
            }

            node* copy() const {
                return new boolean(value);
            }

            void dump(std::ostream& stream) const { stream << (value ? "true" : "false"); };

            const bool& get_value() const { return value; }
    };
    
    template<> struct type_converter<bool> { enum { node_type_code = boolean_node }; typedef boolean node_class; };

    class binary : public node {
        public:
            struct buffer {
                buffer(const buffer& other) : owned(true) { 
                    length = other.length;
                    data = new unsigned char[length];
                    std::memcpy(data, other.data, length);
                }

                buffer(void* data, size_t length) : data(data), length(length), owned(false) { }

                ~buffer() {
                    if (owned)
                        delete[] reinterpret_cast<unsigned char*>(data);
                }

                void dump(std::ostream& stream) const { stream << "<binary: " << length << " bytes>"; };
                
                void* data;
                size_t length;
                bool owned;
            };

        private:
            buffer value;

        public:
            binary(const buffer& buffer) : value(buffer) { }

            binary(const void* const buffer, const size_t count, const bool create = false) : value(NULL, 0) {
                const unsigned char* byte_buffer = reinterpret_cast<const unsigned char*>(buffer);

                if (create) {
                    value.length = count;
                    value.data = new unsigned char[value.length];
                    std::memcpy(value.data, byte_buffer, value.length);
                }
                else {
                    value.length = *reinterpret_cast<const int*>(byte_buffer);
                    value.data = new unsigned char[value.length];
                    std::memcpy(value.data, byte_buffer + 5, value.length);
                }
                
                value.owned = true;
            };

            void serialize(void* const buffer, const size_t count) const {
                unsigned char* byte_buffer = reinterpret_cast<unsigned char*>(buffer);

                *reinterpret_cast<int*>(byte_buffer) = value.length;
                std::memcpy(byte_buffer + 5, value.data, value.length);
            }

            size_t get_serialized_size() const {
                return 5 + value.length;
            }

            unsigned char get_node_code() const {
                return binary_node;
            }

            node* copy() const {
                return new binary(value.data, value.length, true);
            }

            void dump(std::ostream& stream) const { value.dump(stream); };

            const buffer get_value() const { return value; }
    };
    
    template<> struct type_converter< binary::buffer > { enum { node_type_code = binary_node }; typedef binary node_class; };
    
    // Composite types

    class element_list : protected std::map<std::string, node*>, public node {
        public:
            typedef std::map<std::string, node*>::const_iterator const_iterator;

            element_list() { }

            element_list(const element_list& other) {
                for (const_iterator i = other.begin(); i != other.end(); i++)
                    (*this)[i->first] = i->second->copy();
            }

            element_list(const void* const buffer, const size_t count) {
                const unsigned char* byte_buffer = reinterpret_cast<const unsigned char*>(buffer);
                size_t position = 0;

                while (position < count) {
                    node_type type = static_cast<node_type>(byte_buffer[position++]);
                    std::string name(reinterpret_cast<const char*>(byte_buffer + position));
                    node* node = NULL;

                    position += name.length() + 1;
                    node = node::create(type, byte_buffer + position, count - position);

                    if (node != NULL) {
                        position += node->get_serialized_size();
                        (*this)[name] = node;
                    }
                    else
                        break;
                }
            }

            void serialize(void* const buffer, const size_t count) const {
                unsigned char* byte_buffer = reinterpret_cast<unsigned char*>(buffer);
                int position = 0;

                for (const_iterator i = begin(); i != end(); i++) {
                    // Header
                    byte_buffer[position] = i->second->get_node_code();
                    position++;
                    // Key
                    std::strcpy(reinterpret_cast<char*>(byte_buffer + position), i->first.c_str());
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

            node* copy() const {
                return new element_list(*this);
            }

            void dump(std::ostream& stream) const {
                stream << "{ ";

                for (const_iterator i = begin(); i != end(); i++) {
                    stream << "\"" << i->first << "\": ";
                    i->second->dump(stream);

                    if (++i != end())
                        stream << ", ";
                    --i;
                }

                stream << " }";
            }

            void dump(std::ostream& stream, const int level) const {
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
                return std::map<std::string, node*>::begin();
            }

            const_iterator end() const {
                return std::map<std::string, node*>::end();
            }

            bool contains(const std::string& key) const {
                return (std::map<std::string, node*>::find(key) != end());
            }
            
            template<typename T>
            bool contains(const std::string& key) const {
                const_iterator position = std::map<std::string, node*>::find(key);
                return (position != end()) && (position->second->get_node_code() == type_converter<T>::node_type_code);
            }

            ~element_list() {
                for (const_iterator i = begin(); i != end(); i++)
                    delete i->second;
            }

    };
   
    class document : public element_list {
        public:
            document() { }

            document(const void* const buffer, const size_t count) : element_list(reinterpret_cast<const unsigned char*>(buffer) + 4, *reinterpret_cast<const int*>(buffer) - 4 - 1) { }

            void serialize(void* const buffer, const size_t count) const {
                size_t serialized_size = get_serialized_size();

                if (count >= serialized_size) {
                    unsigned char* byte_buffer = reinterpret_cast<unsigned char*>(buffer);

                    *reinterpret_cast<int*>(buffer) = serialized_size;
                    element_list::serialize(byte_buffer + 4, count - 4 - 1);
                    byte_buffer[4 + element_list::get_serialized_size()] = 0;
                }
            }

            size_t get_serialized_size() const {
                return 4 + element_list::get_serialized_size() + 1;
            }

            unsigned char get_node_code() const {
                return document_node;
            }

            node* copy() const {
                return new document(*this);
            }

            template<typename result_type>
            const result_type get(const std::string& key, const result_type& _default) const {
                const node_type node_type_code = static_cast<node_type>(type_converter<result_type>::node_type_code);
                typedef typename type_converter<result_type>::node_class node_class;

                if ((find(key) != end()) && (at(key)->get_node_code() == node_type_code))
                    return reinterpret_cast<const node_class*>(at(key))->get_value();
                else
                    return _default;
            }
            
            const document& get(const std::string& key, const document& _default) const {
                if ((find(key) != end()) && (at(key)->get_node_code() == document_node))
                    return *reinterpret_cast<const document*>(at(key));
                else
                    return _default;
            }

            const std::string get(const std::string& key, const char* _default) const {
                if ((find(key) != end()) && (at(key)->get_node_code() == string_node))
                    return reinterpret_cast<const string*>(at(key))->get_value();
                else
                    return std::string(_default);
            }

            template<typename value_type>
            document& set(const std::string& key, const value_type& value) {
                typedef typename type_converter<value_type>::node_class node_class;

                if (find(key) != end())
                    delete (*this)[key];
                
                (*this)[key] = new node_class(value);
                return (*this);
            }
            
            document& set(const std::string& key, const char* value) {
                if (find(key) != end())
                    delete (*this)[key];

                (*this)[key] = new string(value);
                return (*this);
            }
            
            document& set(const std::string& key, const document& value) {
                if (find(key) != end())
                    delete (*this)[key];

                (*this)[key] = value.copy();
                return (*this);
            }
            
            document& set(const std::string& key) {
                if (find(key) != end())
                    delete (*this)[key];

                (*this)[key] = new null();
                return (*this);
            }
    };
    
    template<> struct type_converter< document > { enum { node_type_code = document_node }; typedef document node_class; };
    
    inline node* node::create(node_type type, const void * const buffer, const size_t count) {
        switch (type) {
            case null_node: return new null();
            case int32_node: return new int32(buffer, count);
            case int64_node: return new int64(buffer, count);
            case double_node: return new Double(buffer, count);
            case document_node: return new document(buffer, count);
            case string_node: return new string(buffer, count);
            case binary_node: return new binary(buffer, count);
            case boolean_node: return new boolean(buffer, count);
            default: return NULL;
        }
    }
}
