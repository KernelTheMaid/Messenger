
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra
LDFLAGS  ?=
LIBS     ?=
TARGET   = messenger
SOURCES   = main.cpp p2p.cpp
OBJECTS   = $(SOURCES:.cpp=.o)

# Определяем ОС
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)


ifeq ($(UNAME_S),Linux)
    CXX = g++
    CXXFLAGS += -DUNIX
    LIBS = -lsfml-network -lsfml-system -pthread
    # Проверка на нативную SFML
    SFML_PATH = Libraries/SFML-3.0.2
    ifneq ($(wildcard $(SFML_PATH)/include),)
        CXXFLAGS += -I$(SFML_PATH)/include
        LDFLAGS  += -L$(SFML_PATH)/lib
    endif
    

else ifeq ($(UNAME_S),FreeBSD)
    CXX = clang++
    CXXFLAGS += -DUNIX -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
    LIBS = -lsfml-network -lsfml-system -pthread
    SFML_PATH = Libraries/SFML-3.0.2
    ifneq ($(wildcard $(SFML_PATH)/include),)
        CXXFLAGS += -I$(SFML_PATH)/include
        LDFLAGS  += -L$(SFML_PATH)/lib
    endif

else ifneq (,$(findstring MINGW,$(UNAME_S)))
    CXX = x86_64-w64-mingw32-g++
    CXXFLAGS += -DWINDOWS
    SOURCES += bluetooth.cpp          
    LIBS = -lsfml-network -lsfml-system -lws2_32 -lbthprops -luser32 -ladvapi32
    SFML_PATH = Libraries/SFML-3.0.2
    CXXFLAGS += -I$(SFML_PATH)/include
    LDFLAGS  += -L$(SFML_PATH)/lib
    TARGET := $(TARGET).exe

else
    $(error Unknown OS. Please set OS manually: make OS=linux|freebsd|win)
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
