# Desktop build of the synth engine for listening tests (see render.cpp).
CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter

render: render.cpp $(wildcard ../code/src/synth/*.h) ../code/src/reverb.h ../code/src/fx_engine.h
	$(CXX) $(CXXFLAGS) -o $@ render.cpp

clean:
	rm -f render
