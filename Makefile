BIN = bareclaw

all: $(BIN)

$(BIN): src/main.asm
	fasm src/main.asm $(BIN)

test:
	@echo "no tests yet"

clean:
	rm -f $(BIN)

.PHONY: all test clean
