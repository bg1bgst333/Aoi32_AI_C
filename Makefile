# ============================================================
# Win32 テキストエディタ - Makefile
# ツールチェーン: MinGW-w64 (g++)
# 規格       : C++03
# ============================================================

CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++03 -DUNICODE -D_UNICODE -mwindows
LDFLAGS  = -mwindows
LIBS     = -lcomctl32 -lcomdlg32

TARGET   = Aoi32_C_AI.exe
SRC      = src/main.cpp
OBJ      = src/main.o

# ============================================================
# ターゲット
# ============================================================

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJ): $(SRC) src/resource.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
