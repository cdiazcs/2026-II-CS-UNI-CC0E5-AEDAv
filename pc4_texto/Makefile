CXX ?= g++
CXXFLAGS ?= -std=c++23 -Wall -Wextra -pedantic -O2

.PHONY: all test test-depth test-depth-audit test-known-defects clean
all: texto
texto: main.cpp texto_excel.h
	$(CXX) $(CXXFLAGS) main.cpp -o texto

test_texto: tests/test_texto.cpp texto_excel.h
	$(CXX) $(CXXFLAGS) tests/test_texto.cpp -o test_texto

test: test_texto
	./test_texto

test-depth: DEPTH_CASES = 100-dependencias 1000-dependencias 5000-dependencias
test-known-defects: DEPTH_CASES = 20000-dependencias

test-depth test-known-defects: test_texto
	@ulimit -c 0; failed=0; \
	for test in $(DEPTH_CASES); do \
		status=0; /usr/bin/time -f "$$test: %e s reales" ./test_texto "$$test" || status=$$?; \
		printf '%s: salida %s\n' "$$test" "$$status"; \
		if [ "$$status" -ne 0 ]; then failed=1; fi; \
	done; exit "$$failed"

test-depth-audit: test_texto
	@ulimit -c 0; failed=0; \
	for formula in binaria parentesis suma-escalar precedencia unaria rango; do \
		for depth in 100 1000 5000 20000; do \
			status=0; /usr/bin/time -f "$$formula $$depth: %e s reales" \
				./test_texto "$$formula" "$$depth" || status=$$?; \
			printf '%s %s: salida %s\n' "$$formula" "$$depth" "$$status"; \
			if [ "$$status" -ne 0 ]; then failed=1; fi; \
		done; \
	done; exit "$$failed"

clean:
	rm -f texto test_texto
