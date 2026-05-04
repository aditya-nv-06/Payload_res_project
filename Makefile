# Makefile – pgsql_ids: PostgreSQL payload fragmentation + SQLi detection sensor
#
# Targets:
#   make              – build without libpq (anomaly + rule detection only)
#   make WITH_LIBPQ=1 – build with pg_stat_activity correlation via libpq
#   make test         – compile and run unit tests
#   make clean        – remove build artefacts

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2 -g \
           -D_GNU_SOURCE \
           -Isrc
LDFLAGS := -lpcap -lm

PREFIX  ?= $(HOME)/.local
BINDIR  ?= $(PREFIX)/bin

TARGET  := pgsql_ids
TEST_TARGET := test_detector

SRCS := src/common/util.c \
        src/app/cli.c     \
        src/net/capture.c \
        src/net/reassembly.c \
        src/net/pg_parser.c \
        src/net/packet_parse.c \
        src/analysis/detector.c \
        src/analysis/ngram.c \
        src/analysis/query_eval.c \
        src/db/db_session.c \
        src/db/pg_correlate.c \
        src/output/alert.c \
        src/main.c

TEST_SRCS := tests/test_detector.c \
             src/common/util.c     \
             src/analysis/detector.c \
             src/analysis/ngram.c   \
             src/output/alert.c     \
             src/db/pg_correlate.c  \
             src/net/reassembly.c

# --------------------------------------------------------------------------- #
# Optional libpq support                                                      #
# --------------------------------------------------------------------------- #

ifdef WITH_LIBPQ
  CFLAGS  += -DWITH_LIBPQ $(shell pg_config --cppflags 2>/dev/null)
  LDFLAGS += $(shell pg_config --ldflags 2>/dev/null) -lpq
endif

# --------------------------------------------------------------------------- #
# Build rules                                                                 #
# --------------------------------------------------------------------------- #

.PHONY: all test clean install uninstall

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built $@"

$(TEST_TARGET): $(TEST_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) *.o
	@echo "Cleaned"

install: $(TARGET) pqCheck
	@mkdir -p $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	install -m 755 pqCheck $(BINDIR)/pqCheck
	@echo "Installed to $(BINDIR)"

uninstall:
	rm -f $(BINDIR)/$(TARGET) $(BINDIR)/pqCheck
	@echo "Removed from $(BINDIR)"

# Show effective build flags
info:
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "SRCS    = $(SRCS)"
