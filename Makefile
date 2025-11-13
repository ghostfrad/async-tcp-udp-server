CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGET = async_server
SOURCES = main.cpp server.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

install: $(TARGET)
	install -d /usr/local/bin/
	install -m 755 $(TARGET) /usr/local/bin/
	install -d /etc/async-server/
	install -m 644 async-server.service /lib/systemd/system/

uninstall:
	rm -f /usr/local/bin/$(TARGET)
	rm -f /lib/systemd/system/async-server.service
	systemctl daemon-reload
