objects = intel_8080.o

CFLAGS := -std=gnu99 -Wall -Werror -fpic

libname = libi8080.so

all: $(libname)

$(libname): $(objects)
	$(CC) -shared -o $@ $^

clean:
	-@$(RM) $(libname) $(DEBUGGER) *.o *.d

.PHONY: all clean
