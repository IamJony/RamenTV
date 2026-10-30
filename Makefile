# Makefile para RamenTV
# Ubicación: Raíz del proyecto. Los fuentes están en src/

SRC      = src
CXX      = g++
CC       = gcc

# -fno-rtti es CRÍTICO en Tiny Core para evitar errores de typeinfo con FLTK
# Usamos c++17 para soporte completo de regex y características modernas
CXXFLAGS = $(shell fltk-config --cxxflags 2>/dev/null) -std=c++17 -O2 -Wall -fno-rtti
CFLAGS   = -O2 -Wall

# Enlazado: FLTK base + FLTK imágenes + matemáticas + hilos + libstdc++
LDFLAGS  = $(shell fltk-config --ldflags 2>/dev/null) -lfltk_images -lm -lpthread -lstdc++

OBJS   = $(SRC)/RamenTV-GUI.o $(SRC)/themes.o $(SRC)/library.o $(SRC)/cJSON.o
TARGET = RamenTV

all: $(TARGET)

# Enlazado final (siempre con g++)
$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

# Compilación de Palomitas-GUI.cpp (El nombre del archivo fuente debe coincidir)
$(SRC)/Palomitas-GUI.o: $(SRC)/Palomitas-GUI.cpp $(SRC)/themes.h $(SRC)/library.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilación de themes.cpp
$(SRC)/themes.o: $(SRC)/themes.cpp $(SRC)/themes.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilación de library.cpp
$(SRC)/library.o: $(SRC)/library.cpp $(SRC)/library.h $(SRC)/cJSON.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilación de cJSON (Usa CFLAGS porque es C puro)
$(SRC)/cJSON.o: $(SRC)/cJSON.c $(SRC)/cJSON.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(SRC)/*.o $(TARGET)

.PHONY: all clean run
