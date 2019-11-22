objects = intel_8080.o intel_8080_test.o

CFLAGS := -std=gnu99 -Wall

libname = intel_8080

all: $(libname)

$(libname): $(objects)
	$(CC) -o $@ $^

clean:
	-@$(RM) $(libname) $(DEBUGGER) *.o *.d

.PHONY: all clean
