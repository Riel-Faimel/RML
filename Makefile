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

LDFLAGS := -mwindows -L$(QT_DIR)/lib -static
LDLIBS  := -lQt6Quick -lQt6QuickControls2 -lQt6Qml -lQt6Gui -lQt6Core -lwinhttp -static-libgcc -static-libstdc++

ifeq ($(CONFIG),release)
CXXFLAGS := -std=c++20 -O2 -DNDEBUG -DQT_NO_DEBUG -MMD -MP
else
CXXFLAGS := -std=c++20 -O0 -g -DRML_DEBUG -MMD -MP
endif

CORE_SOURCES := src/app/tools/Json.cpp \
                src/app/tools/Json.Parser.cpp \
                src/app/tools/Path.cpp \
                src/app/network/Network.cpp \
                src/app/tools/Process.cpp \
                src/app/tools/Sha1.cpp \
                src/app/tools/Compat.cpp \
                src/app/json/JsonArena.cpp \
                src/app/directory/Directory.cpp \
                src/app/directory/Cores.cpp \
                src/app/directory/Cores.Scan.cpp \
                src/app/directory/Migration.cpp \
                src/app/directory/Migration/Journal.cpp \
                src/app/directory/Migration/Plan.cpp \
                src/app/directory/Versions.cpp \
                src/app/nettask/NetTask.cpp \
                src/app/launch/Version/Version.cpp \
                src/app/launch/Environment/Environment.cpp \
                src/app/launch/Starter/Starter.cpp \
                src/app/download/VersionSource/Sources/Sources.cpp \
                src/app/download/VersionSource/Cache/Cache.cpp \
                src/app/download/VersionSource/Mojang/Mojang.cpp \
                src/app/download/VersionSource/Loader/Loader.cpp \
                src/app/download/VersionSource/Forge/Forge.cpp \
                src/app/download/VersionSource/NeoForge/NeoForge.cpp \
                src/app/download/VersionSource/Modrinth/Modrinth.cpp \
                src/app/download/VersionSource/CurseForge/CurseForge.cpp \
                src/app/download/VersionSource/VersionSource.cpp \
                src/app/directory/Installer/Transfer/Transfer.cpp \
                src/app/directory/Installer/Placement/Placement.cpp \
                src/app/directory/Installer/Version/Version.cpp \
                src/app/directory/Flow/Support.cpp \
                src/app/directory/Flow/Transaction.cpp \
                src/app/directory/Flow/Processors.cpp \
                src/app/directory/Flow/Profile.cpp \
                src/app/directory/Flow/Vanilla.cpp \
                src/app/directory/Flow/Fabric.cpp \
                src/app/directory/Flow/Forge.cpp \
                src/app/directory/Flow/NeoForge.cpp \
                src/app/directory/Flow/Dispatcher.cpp \
                src/app/server/Catalog/Catalog.cpp \
                src/app/server/Runner/Runner.cpp \
                src/app/server/Installer/Installer.cpp \
                src/app/server/Installer/Installer.Forge.cpp \
                src/app/server/Properties/Properties.cpp \
                src/app/server/Runtime/Control.cpp \
                src/app/server/Naming/Name.cpp \
                src/app/server/Server.cpp \
                src/app/server/Server.Maintenance.cpp

APP_SOURCES  := src/app/main.cpp src/app/Bridge.cpp src/app/Bridge.Maintenance.cpp \
                src/app/ServerBridge.cpp \
                src/app/bridge/Labels.cpp \
                src/app/bridge/Background.cpp \
                src/app/bridge/SourceRecords.cpp \
                src/app/bridge/Mods.cpp \
                src/app/bridge/Versions.cpp \
                src/app/bridge/Tasks.cpp \
                src/app/bridge/Downloads.cpp \
                src/app/bridge/Content.cpp

CORE_OBJECTS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(CORE_SOURCES))
APP_OBJECTS  := $(patsubst src/%.cpp,$(BUILD)/%.o,$(APP_SOURCES)) \
                $(BUILD)/moc_Bridge.o $(BUILD)/moc_ServerBridge.o

TARGET := $(BUILD)/rml.exe
CHECK  := $(BUILD)/rml_check.exe
CORES_CHECK := $(BUILD)/rml_cores_test.exe

all: $(TARGET)

$(BUILD):
	@ if not exist "$(BUILD)" mkdir "$(BUILD)"

$(BUILD)/moc_Bridge.cpp: src/app/Bridge.h | $(BUILD)
	@ echo   MOC  $<
	@ $(MOC) $< -o $@

$(BUILD)/moc_ServerBridge.cpp: src/app/ServerBridge.h | $(BUILD)
	@ echo   MOC  $<
	@ $(MOC) $< -o $@

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	@ if not exist "$(dir $@)" mkdir "$(dir $@)"
	@ echo   CXX  $<
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/moc_Bridge.o: $(BUILD)/moc_Bridge.cpp | $(BUILD)
	@ echo   CXX  $(BUILD)/moc_Bridge.cpp
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/moc_ServerBridge.o: $(BUILD)/moc_ServerBridge.cpp | $(BUILD)
	@ echo   CXX  $(BUILD)/moc_ServerBridge.cpp
	@ $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(CORE_OBJECTS) $(APP_OBJECTS)
	@ echo   LINK $@
	@ $(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

# 装配检查：同样的模块，不开窗口
$(CHECK): $(CORE_OBJECTS) $(BUILD)/test/main.o
	@ echo   LINK $@
	@ $(CXX) $^ -L$(QT_DIR)/lib -lshell32 -lwinhttp -o $@

check: $(CHECK)
	"$(CHECK)"

# 核心/入口分离与引用测试：全部在 build/ 临时目录，不碰真实 .minecraft
$(CORES_CHECK): $(CORE_OBJECTS) $(BUILD)/test/cores_test.o
	@ echo   LINK $@
	@ $(CXX) $^ -L$(QT_DIR)/lib -lwinhttp -lshell32 -o $@

cores-test: $(CORES_CHECK)
	"$(CORES_CHECK)"

run: $(TARGET)
	"$(TARGET)"

# 部署到项目根下的 dist/：固定用 release，避免把调试构建发出去
DIST := dist

deploy:
	@ echo   BUILD release
	@ $(MAKE) -s --no-print-directory CONFIG=release all
	@ echo   DEPLOY dist
	@ tools\deploy.bat "$(CURDIR)/build/release/rml.exe" "$(QT_DIR)" > build\deploy.log 2>&1 || (echo [deploy] failed, see build\deploy.log & exit /b 1)

clean:
	@ if exist "build" rmdir /s /q "build"
	@ if exist "$(DIST)" rmdir /s /q "$(DIST)"

.PHONY: all run check cores-test deploy clean

# 头文件依赖：改了 .h 也要重编，否则会因对象布局不一致而崩
-include $(CORE_OBJECTS:.o=.d) $(APP_OBJECTS:.o=.d) $(BUILD)/test/main.d $(BUILD)/test/cores_test.d
