# Makefile para ramentv

SRC      = src
CXX      = g++
CC       = gcc

CXXFLAGS = $(shell fltk-config --cxxflags 2>/dev/null) -std=c++17 -O2 -Wall -fno-rtti
CFLAGS   = -O2 -Wall


LDFLAGS  = $(shell fltk-config --ldflags 2>/dev/null) -lfltk_images -lm -lpthread -lstdc++

OBJS   = $(SRC)/ramentv-GUI.o $(SRC)/themes.o $(SRC)/library.o $(SRC)/cJSON.o
TARGET = ramentv

all: $(TARGET)

# Enlazado final (siempre con g++)
$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)


$(SRC)/ramentv-GUI.o: $(SRC)/ramentv-GUI.cpp $(SRC)/themes.h $(SRC)/library.h
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(SRC)/themes.o: $(SRC)/themes.cpp $(SRC)/themes.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC)/library.o: $(SRC)/library.cpp $(SRC)/library.h $(SRC)/cJSON.h
	$(CXX) $(CXXFLAGS) -c $< -o $@


$(SRC)/cJSON.o: $(SRC)/cJSON.c $(SRC)/cJSON.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(SRC)/*.o $(TARGET)

.PHONY: all clean run
