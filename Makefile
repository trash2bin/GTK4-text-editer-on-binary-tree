# Makefile - простая обёртка для сборки через cmake
BUILD_DIR := build
CMAKE := cmake

NPROC := $(shell nproc 2>/dev/null || echo 2)

.PHONY: all clean editor test debug release rebuild

all: editor

editor:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Release .. \
	  && $(CMAKE) --build . --parallel $(NPROC)

debug:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Debug .. \
	  && $(CMAKE) --build . --parallel $(NPROC)

test:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=Debug .. \
	  && $(CMAKE) --build . --parallel $(NPROC) \
	  && cd $(BUILD_DIR) && ctest --output-on-failure -j1

# Релиз с максимально возможными оптимизациями (можно отключить march=native)
release:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && \
	  $(CMAKE) -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_MAX_OPTIM=ON .. \
	  && $(CMAKE) --build . --parallel $(NPROC) --config RelWithDebInfo

# С переносимостью 
portable_release:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && \
	  $(CMAKE) -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_PORTABLE_OPTIM=ON .. \
	  && $(CMAKE) --build . --parallel $(NPROC) --config RelWithDebInfo

clean:
	-rm -rf $(BUILD_DIR)

rebuild: clean all
