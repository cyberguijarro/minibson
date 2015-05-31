# minibson

Zero-dependency C++ BSON libraries.

## Features

 * C++03 compliant
 * Two flavours tailored for perfomance and footprint
 * Supports double, string, document, binary, boolean, null, 32-bit integer and 64-bit integer types
 * Header-only files (this may change soon)

## minibson

minibson is a DOM-style BSON implementation allowing create, update and delete operations at any level of the document. Internally, it uses a node tree where each node is dinamically allocated. Deserialization builds a new tree from the input datastream, and serialization compresses the tree into a datastream.

## microbson

microbson is a much more efficient implementation, where no additional memory is used to keep track of document nodes. All fields are directly read from the datastream, which is traversed during each query. No insertions, modifications or deletions are yet supported.

## Which one should I use?

 * If your code creates or updates documents, you'll have to stick with minibson
 * If all you want is to query documents (read-only), microbson is probably your best choice. The only exception might be very large documents with lots of keys in each level (microbson lookups are linear, while minibson indexes allow lookups in logarithmic times)

## Future improvements

 * Endianness support
 * Header/implementation file split
 * Mutability support in microbson

