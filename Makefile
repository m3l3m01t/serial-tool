BINS := fusion-tool
all: $(BINS)

CFLAGS := -Wall -ggdb

fusion-tool: sbuf.o fusion-tool.o

.PHONY: clean
clean:
	$(RM) $(BINS) *.o *.a *.bin tags
