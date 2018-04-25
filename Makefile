LDIR=lib
ODIR=build

CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra -fsanitize=address -DBE_DEBUG=0
CXXFLAGS= -std=c++11 -Wall -Wextra -fsanitize=address -I$(LDIR)

LIBS=-lcurl

_DEPS=bencode.h tracker.h torrentparser.h
DEPS=$(patsubst %,$(LDIR)/%,$(_DEPS))

_OBJ=bencode.o tracker.o torrentparser.o atorrent-cli.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: $(LDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/%.o: $(LDIR)/%.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS)

# Used for atorrent-cli.cpp in the main directory
$(ODIR)/%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

atorrent-cli: $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(ODIR)/*.o

.PHONY: clean
