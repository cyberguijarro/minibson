CXXFLAGS=-std=c++03 -Wall -g -O0 
HEADERS=minibson.hpp microbson.hpp
TEST=test.cpp

test: $(TEST) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(TEST) -o $@

check: test
	./$^

memcheck: test
	valgrind --leak-check=full ./$^

clean:
	$(RM) test
