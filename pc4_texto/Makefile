CXX ?= g++
CXXFLAGS ?= -std=c++23 -Wall -Wextra -pedantic -O2
# -I.. para encontrar containers/ desde la raiz del repo
CPPFLAGS += -I..

.PHONY: all test test-depth clean

# programa interactivo
all: texto

texto: main.cpp texto_excel.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) main.cpp -o texto

test_texto: tests/test_texto.cpp texto_excel.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_texto.cpp -o test_texto

# pruebas normales
test: test_texto
	./test_texto

# cadenas de 100, 1000 y 5000 celdas (mas de 1000 debe dar "Demasiadas dependencias")
test-depth: test_texto
	./test_texto profundidad

clean:
	rm -f texto test_texto
