LDIR=lib
ODIR=build

CC=gcc
CXX=g++
CFLAGS=-Wall -g -fsanitize=address -Wextra -W -DBE_DEBUG=0
CXXFLAGS=-Wall -g -std=c++11 -fsanitize=address -Wextra -W -I$(LDIR)

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
