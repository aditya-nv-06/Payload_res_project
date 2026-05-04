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

TARGET  := pgsql_ids
TEST_TARGET := test_detector

SRCS := src/util.c      \
        src/capture.c   \
        src/reassembly.c\
        src/pg_parser.c \
        src/detector.c  \
        src/ngram.c     \
  src/query_eval.c \
  src/db_session.c \
        src/pg_correlate.c \
        src/alert.c     \
        src/main.c

TEST_SRCS := tests/test_detector.c \
             src/util.c            \
             src/detector.c        \
             src/ngram.c           \
             src/alert.c           \
             src/pg_correlate.c    \
             src/reassembly.c

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

.PHONY: all test clean

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

# Show effective build flags
info:
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "SRCS    = $(SRCS)"
