# Název binárního souboru
TARGET = sauna

# Kompilátor a jeho příznaky
CXX = g++
CXXFLAGS = -g -O0 -std=c++11 -Wall -Wextra -pedantic
LDFLAGS = -lsimlib -lm

# Zdrojové soubory
SRC = sauna.cpp

# Pravidlo pro sestavení binárního souboru
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# Vyčištění vytvořených souborů
clean:
	rm -f $(TARGET)
