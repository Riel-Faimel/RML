# RML — Makefile（mingw32-make）
#
#   mingw32-make                    调试构建
#   mingw32-make CONFIG=release     交付构建
#   mingw32-make -j12               12 核并行
#   mingw32-make check              装配检查（不开窗口）
#   mingw32-make run
#   mingw32-make clean

QT_DIR ?= D:/Qt/6.10.2/mingw_64
CXX    ?= g++
MOC    ?= $(QT_DIR)/bin/moc.exe
CONFIG ?= debug
BUILD  ?= build/$(CONFIG)

# 命令都用 cmd 跑，避免 PATH 里的 sh 抢走 copy/mkdir 这些内建命令
SHELL := cmd.exe

CPPFLAGS := -I. -Isrc -I$(BUILD) \
            -I$(QT_DIR)/include \
            -I$(QT_DIR)/include/QtCore \
            -I$(QT_DIR)/include/QtGui \
            -I$(QT_DIR)/include/QtQml \
            -I$(QT_DIR)/include/QtQuick \
            -I$(QT_DIR)/include/QtQuickControls2 \
            -DRML_QML_DIR=\"$(CURDIR)/src/qml\" \
            -DRML_JSON_DIR=\"$(CURDIR)/src/json\"

LDFLAGS := -mwindows -L$(QT_DIR)/lib
LDLIBS  := -lQt6Quick -lQt6QuickControls2 -lQt6Qml -lQt6Gui -lQt6Core -lwinhttp -static-libgcc -static-libstdc++

ifeq ($(CONFIG),release)
CXXFLAGS := -std=c++20 -O2 -DNDEBUG -DQT_NO_DEBUG -MMD -MP
else
CXXFLAGS := -std=c++20 -O0 -g -DRML_DEBUG -MMD -MP
endif

CORE_SOURCES := src/app/tools/Json.cpp \
                src/app/tools/tools.cpp \
                src/app/launch/Version/Version.cpp \
                src/app/launch/Environment/Environment.cpp \
                src/app/launch/Starter/Starter.cpp \
                src/app/download/VersionSource/Cache/Cache.cpp \
                src/app/download/VersionSource/Mojang/Mojang.cpp \
                src/app/download/VersionSource/Loader/Loader.cpp \
                src/app/download/VersionSource/Modrinth/Modrinth.cpp \
                src/app/download/VersionSource/CurseForge/CurseForge.cpp \
                src/app/download/VersionSource/VersionSource.cpp \
                src/app/download/Installer/Transfer/Transfer.cpp \
                src/app/download/Installer/Placement/Placement.cpp \
                src/app/download/Installer/Version/Version.cpp

APP_SOURCES  := src/app/main.cpp src/app/Bridge.cpp

CORE_OBJECTS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(CORE_SOURCES))
APP_OBJECTS  := $(patsubst src/%.cpp,$(BUILD)/%.o,$(APP_SOURCES)) $(BUILD)/moc_Bridge.o

TARGET := $(BUILD)/rml.exe
CHECK  := $(BUILD)/rml_check.exe

all: $(TARGET)

$(BUILD):
	@ if not exist "$(BUILD)" mkdir "$(BUILD)"

$(BUILD)/moc_Bridge.cpp: src/app/Bridge.h | $(BUILD)
	@ $(MOC) $< -o $@

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	@ if not exist "$(dir $@)" mkdir "$(dir $@)"
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/moc_Bridge.o: $(BUILD)/moc_Bridge.cpp | $(BUILD)
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(CORE_OBJECTS) $(APP_OBJECTS)
	@ $(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

# 装配检查：同样的模块，不开窗口
$(CHECK): $(CORE_OBJECTS) $(BUILD)/test/main.o
	@ $(CXX) $^ -L$(QT_DIR)/lib -lshell32 -lwinhttp -o $@

check: $(CHECK)
	"$(CHECK)"

run: $(TARGET)
	"$(TARGET)"

# 部署到项目根下的 dist/：固定用 release，避免把调试构建发出去
DIST := dist

deploy:
	@ $(MAKE) CONFIG=release all
	@ cmd /c tools\deploy.bat "$(CURDIR)/build/release/rml.exe" "$(QT_DIR)"

clean:
	@ if exist "build" rmdir /s /q "build"
	@ if exist "$(DIST)" rmdir /s /q "$(DIST)"

.PHONY: all run check deploy clean

# 头文件依赖：改了 .h 也要重编，否则会因对象布局不一致而崩
-include $(CORE_OBJECTS:.o=.d) $(APP_OBJECTS:.o=.d) $(BUILD)/test/main.d
