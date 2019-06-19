TARGETS=proxy-daemon

all: $(TARGETS)
clean:
	rm -f $(TARGETS) *~

proxy-daemon: proxy-daemon.cpp
	g++ -std=c++11 -pthread -g -o proxy-daemon proxy-daemon.cpp
