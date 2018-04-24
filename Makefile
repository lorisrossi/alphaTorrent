CC=gcc
CXX=g++
CFLAGS=-Wall -g -fsanitize=address -Wextra -W -DBE_DEBUG=0
CXXFLAGS=-Wall -g -std=c++11 -fsanitize=address -Wextra -W
DEPS=bencode.h
OBJ=bencode.o atorrent-cli.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

atorrent-cli: $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS)

clean:
	rm -f *.o

.PHONY: clean
