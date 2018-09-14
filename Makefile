objects = intel_8080.o intel_8080_test.o

CFLAGS += -std=gnu99 -Wall -O2

EMULATOR = emulator

all: $(EMULATOR)

$(EMULATOR): $(objects)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	-@$(RM) $(EMULATOR) $(DEBUGGER) *.o *.d

.PHONY: all clean
