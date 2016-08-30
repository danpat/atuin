# inherit from env if set
CC := $(CC)
CXX := $(CXX)
CXXFLAGS := $(CXXFLAGS)
LDFLAGS := $(LDFLAGS)

MASON ?= .mason/mason
MASON_HOME ?= mason_packages/.link
MASON_FLAGS ?= -isystem$(MASON_HOME)/include -L$(MASON_HOME)/lib

all: bin/server

$(MASON):
	    git submodule update --init

bin:
	mkdir -p bin

bin/server: src/server.cpp src/tile.hpp src/vector_tile.hpp src/web_mercator.hpp mason_packages bin
	$(CXX) -o bin/server src/server.cpp $(MASON_FLAGS) $(CXXFLAGS) $(LDFLAGS) -DNDEBUG -O3 -lpthread -lz -lexpat -lboost_filesystem -lboost_system -lboost_chrono -lboost_regex -std=c++11

bin/decode: decode.cpp mason_packages bin
	$(CXX) -o bin/decode decode.cpp $(MASON_FLAGS) $(CXXFLAGS) $(LDFLAGS) -DNDEBUG -O3 -std=c++11

clean:
	rm -rf bin

# Mason dependencies
$(MASON_HOME)/lib/libboost_regex.a:
	$(MASON) install boost_libregex 1.61.0
	$(MASON) link boost_libregex 1.61.0

$(MASON_HOME)/include/protozero/pbf_writer.hpp:
	$(MASON) install protozero 1.4.0
	$(MASON) link protozero 1.4.0

mason_packages: $(MASON) $(MASON_HOME)/lib/libboost_regex.a $(MASON_HOME)/include/protozero/pbf_writer.hpp
	$(MASON) install boost 1.61.0
	$(MASON) install boost_libprogram_options 1.61.0
	$(MASON) install boost_libfilesystem 1.61.0
	$(MASON) install boost_libsystem 1.61.0
	$(MASON) install boost_libiostreams 1.61.0
	$(MASON) install libosmium 2.8.0
	$(MASON) link boost 1.61.0
	$(MASON) link boost_libprogram_options 1.61.0
	$(MASON) link boost_libfilesystem 1.61.0
	$(MASON) link boost_libsystem 1.61.0
	$(MASON) link libosmium 2.8.0
	$(MASON) link boost_libiostreams 1.61.0
