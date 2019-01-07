CORESOURCES=image/image.cpp transform/transform.cpp maniac/*.cpp encoding/*.cpp io.cpp
SOURCES=$(CORESOURCES) fuif.cpp
COREHFILES=*.h image/*.h transform/*.h maniac/*.h encoding/*.h
HFILES=$(COREHFILES) import/*.h export/*.h

fuif: $(SOURCES) $(HFILES)
	g++ -O2 -DNDEBUG -g0 -std=gnu++17 $(SOURCES) -lpng -ljpeg -o fuif

fuif.prof: $(SOURCES) $(HFILES)
	g++ -O2 -DNDEBUG -ggdb3 -pg -std=gnu++17 $(SOURCES) -lpng -ljpeg -o fuif.prof

fuif.perf: $(SOURCES) $(HFILES)
	g++ -O2 -DNDEBUG -ggdb3 -std=gnu++17 $(SOURCES) -lpng -ljpeg -o fuif.perf


fuif.dbg: $(SOURCES) $(HFILES)
	g++ -DDEBUG -O0 -ggdb3 -std=gnu++17 $(SOURCES) -lpng -ljpeg -o fuif.dbg


fuifplay: $(CORESOURCES) $(COREHFILES) fuifplay.cpp
	g++ -O2 -DNDEBUG -g0  -std=gnu++17  $(CORESOURCES) fuifplay.cpp `pkg-config --cflags --libs sdl2` -o fuifplay
