# Desktop build of the synth engine for listening tests (see render.cpp).
CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter

all: render boottest

render: render.cpp $(wildcard ../code/src/synth/*.h) ../code/src/reverb.h ../code/src/fx_engine.h
	$(CXX) $(CXXFLAGS) -o $@ render.cpp

boottest: boottest.cpp $(wildcard ../code/src/synth/*.h) ../code/src/reverb.h ../code/src/fx_engine.h
	$(CXX) $(CXXFLAGS) -o $@ boottest.cpp

clean:
	rm -f render boottest
