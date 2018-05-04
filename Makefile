LDIR=lib
ODIR=build

CC=gcc
CXX=g++
CFLAGS=-Wall -Wextra -g -fsanitize=address -DBE_DEBUG=0
CXXFLAGS= -std=c++11 -Wall -g -Wextra -fsanitize=address -I$(LDIR)

LIBS=-lssl -lcrypto -lcurl -lpthread -lboost_filesystem -lboost_system -lboost_thread -lglog

_DEPS=bencode.h tracker.h peer.h torrentparser.hpp filehandler.hpp
DEPS=$(patsubst %,$(LDIR)/%,$(_DEPS))

_OBJ=bencode.o tracker.o torrentparser.o atorrent-cli.o filehandler.o peer.o
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
