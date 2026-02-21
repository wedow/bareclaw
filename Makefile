BIN = bareclaw

all: $(BIN)

$(BIN): src/main.asm
	fasm src/main.asm $(BIN)

TEST_SRCS = $(wildcard test/test_*.asm)
TEST_BINS = $(TEST_SRCS:.asm=)

test: $(TEST_BINS)
	@fail=0; \
	for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		./$$t || fail=1; \
	done; \
	exit $$fail

test/%: test/%.asm src/test_macros.inc $(wildcard src/*.inc)
	fasm $< $@

clean:
	rm -f $(BIN) $(TEST_BINS)

.PHONY: all test clean
