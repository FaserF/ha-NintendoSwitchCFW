CXXFLAGS = -std=c++17 -I../include -I. -DUNIT_TEST
DEFINES = 

SOURCES = test_main.cpp test_config.cpp ../ConfigManager.cpp
TARGET = run_tests

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(DEFINES) $^ -o $@

clean:
	rm -f $(TARGET)
