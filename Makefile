objects = intel_8080.o intel_8080_test.o

CFLAGS := -std=gnu99 -Wall -DCC_VERBOSE -O3

testpro = intel_8080_test

all: $(testpro)

$(testpro): $(objects)
	$(CC) -o $@ $^

clean:
	-@$(RM) $(testpro) $(DEBUGGER) *.o *.d

.PHONY: all clean
