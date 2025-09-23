NAME = a.exe
STANDARD = c23
FLAGS = -MMD -pthread -Wall -Wextra -Werror -Wfatal-errors -Wno-unused-function -Ilib 
DB_FLAGS = -g3
STB_DEFS = -D STB_IMAGE_IMPLEMENTATION -D STB_IMAGE_WRITE_IMPLEMENTATION

SOURCES = $(shell find . -name "*.c")
OBJECTS = $(SOURCES:%.c=build/%.o)
DEPENDENCIES = $(OBJECTS:%.o=%.d)

all: debug

debug: build data $(OBJECTS)
	@gcc $(OBJECTS) $(FLAGS) $(DB_FLAGS) -std=$(STANDARD) $(STB_DEFS) -o $(NAME)

build/%.o: %.c
	@mkdir -p $(dir $@)
	@echo $<
	@gcc $(FLAGS) $(DB_FLAGS) -std=$(STANDARD) $(STB_DEFS) -c -o $@ $<

build:
	@mkdir -p build

data:
	@mkdir -p data

-include $(DEPENDENCIES)

.PHONY: clean

clean:
	rm -rf build


