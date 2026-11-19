CC = gcc
CFLAGS = -std=c11 -O0 -Wall -Wextra -Wpedantic -pthread
SOURCES = src/main.c src/segmentation.c src/image.c
.PHONY: all test benchmark report
all: segment.exe
segment.exe: $(SOURCES) src/segmentation.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@
test: segment.exe
	$(CC) $(CFLAGS) -Isrc tests/test_api.c src/segmentation.c src/image.c -o tests/test_api.exe
	./tests/test_api.exe
	$(CC) $(CFLAGS) -Isrc tests/test_launch_failure.c -o tests/test_launch_failure.exe
	./tests/test_launch_failure.exe
	python tests/verify.py
benchmark: test
	python scripts/environment.py
	./segment.exe bench results/raw.csv 100
	python scripts/analyze.py
	python scripts/check_artifacts.py
report:
	xelatex -interaction=nonstopmode -halt-on-error -output-directory=output/pdf report.tex
	xelatex -interaction=nonstopmode -halt-on-error -output-directory=output/pdf report.tex
	xelatex -interaction=nonstopmode -halt-on-error -output-directory=output/pdf report.tex
