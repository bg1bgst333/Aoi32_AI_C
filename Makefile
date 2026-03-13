# ============================================================
# Win32 テキストエディタ - Makefile
# ツールチェーン: MinGW-w64 (g++)
# 規格       : C++03
# ============================================================

CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++03 -DUNICODE -D_UNICODE -mwindows
LDFLAGS  = -mwindows
LIBS     = -lcomctl32 -lcomdlg32

TARGET = Aoi32_AI_C.exe
OBJS   = src/main.o src/MainWnd.o src/FileIO.o

# ============================================================
# ターゲット
# ============================================================

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

src/main.o: src/main.cpp src/MainWnd.h src/resource.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/MainWnd.o: src/MainWnd.cpp src/MainWnd.h src/FileIO.h src/resource.h src/common.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

src/FileIO.o: src/FileIO.cpp src/FileIO.h src/common.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
