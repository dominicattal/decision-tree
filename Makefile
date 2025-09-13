NAME = a.exe
STANDARD = c23
FLAGS = -Wall -Wextra -Werror -Wfatal-errors -Ilib
DB_FLAGS = -g3 -Og
STB_DEFS = -D STB_IMAGE_IMPLEMENTATION -D STB_IMAGE_WRITE_IMPLEMENTATION

SOURCES = $(shell find . -name "*.c")
OBJECTS = $(SOURCES:%.c=build/%.o)
DEPENDENCIES = $(OBJECTS:%.o=%.d)

all: debug

debug: build $(OBJECTS)
	@gcc $(OBJECTS) $(FLAGS) $(DB_FLAGS) -std=$(STANDARD) $(STB_DEFS) -o $(NAME)

build/%.o: %.c
	@mkdir -p $(dir $@)
	@echo $<
	@gcc $(FLAGS) $(DB_FLAGS) -std=$(STANDARD) $(STB_DEFS) -c -o $@ $<

build:
	@mkdir -p build

-include $(DEPENDENCIES)

.PHONY: clean

clean:
	rm -rf build


