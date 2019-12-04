objects = intel_8080.o intel_8080_test.o

CFLAGS := -std=gnu99 -Wall

testpro = intel_8080_test

all: $(testpro)

$(testpro): $(objects)
	$(CC) -o $@ $^

clean:
	-@$(RM) $(testpro) $(DEBUGGER) *.o *.d

.PHONY: all clean
