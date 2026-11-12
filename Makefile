# 暗蛆突围 — C++17 + SDL2, no external assets.
CXX      := /usr/bin/g++
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -Wno-unused-parameter
CPPFLAGS := -Isrc -Ithird_party/SDL2 -MMD -MP
LDFLAGS  :=
LDLIBS   := -Wl,-l:libSDL2-2.0.so.0 -lm

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,build/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
TARGET := build/maggot

.PHONY: all run clean font shots

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

font:
	python3 tools/gen_font.py

shots: $(TARGET)
	@mkdir -p docs/screens
	./$(TARGET) --shot hub docs/screens/hub.bmp
	./$(TARGET) --map 0 --shot deploy docs/screens/deploy.bmp
	./$(TARGET) --map 0 --shot deploy_loadout docs/screens/deploy_loadout.bmp
	./$(TARGET) --map 0 --shot deploy_knife docs/screens/deploy_knife.bmp
	./$(TARGET) --shot stash docs/screens/stash.bmp
	./$(TARGET) --shot missions docs/screens/missions.bmp
	./$(TARGET) --shot shop docs/screens/shop.bmp
	./$(TARGET) --shot shop_armor docs/screens/shop_armor.bmp
	./$(TARGET) --shot shop_bag docs/screens/shop_bag.bmp
	./$(TARGET) --shot shop_supply docs/screens/shop_supply.bmp
	./$(TARGET) --shot settings docs/screens/settings.bmp
	./$(TARGET) --map 0 --shot raid docs/screens/raid.bmp
	./$(TARGET) --map 0 --shot knife docs/screens/knife.bmp
	./$(TARGET) --map 0 --shot map docs/screens/map.bmp
	./$(TARGET) --map 0 --shot pause docs/screens/pause.bmp
	./$(TARGET) --shot result docs/screens/result.bmp
	@for f in docs/screens/*.bmp; do magick "$$f" "$${f%.bmp}.png"; done
	@rm -f docs/screens/*.bmp

clean:
	rm -rf build/*.o build/*.d build/maggot build/maggot.exe

-include $(DEPS)
