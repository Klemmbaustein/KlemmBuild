TARGET_EXEC := KlemmBuild

BUILD_DIR := ./obj
SRC_DIRS := ./Source

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
INC_DIRS := $(SRC_DIRS) json/include
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS := $(INC_FLAGS) -MMD -MP -fno-char8_t -std=c++20

$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@

# Build C++ object files
$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) -c $< -o $@


.PHONY: clean
clean:
	rm -r $(BUILD_DIR)

install: $(TARGET_EXEC)
	$(info Installing to $(DESTDIR)$(PREFIX)/bin/)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 644 ./KlemmBuild $(DESTDIR)$(PREFIX)/bin/
	chmod +x $(DESTDIR)$(PREFIX)/bin/KlemmBuild

# Include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want those
# errors to show up.
-include $(DEPS)