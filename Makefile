CXXFLAGS=-std=c++03 -Wall -g 

test: test.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

check: test
	./$^

memcheck: test
	valgrind --leak-check=full ./$^

clean:
	$(RM) test
