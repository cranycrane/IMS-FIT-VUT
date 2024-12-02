TARGET = sauna

CXX = g++
CXXFLAGS = -g -O0 -std=c++11 -Wall -Wextra -pedantic
LDFLAGS = -lsimlib -lm

SRC = sauna.cpp

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

improved: $(TARGET)
	./$(TARGET) -s 5

pack:
	tar -czvf 09_xjerab28_xteich02.tar.gz $(SRC) Makefile dokumentace.pdf

clean:
	rm -f $(TARGET)


