# Makefile – pqCheck: PostgreSQL payload fragmentation + SQLi detection sensor
#
# Build Targets:
#   make                    – build without libpq (anomaly + rule detection only)
#   make WITH_LIBPQ=1       – build with pg_stat_activity correlation via libpq
#   make WITH_TUI=1         – build with ncurses-based interactive TUI
#   make test               – compile and run unit tests
#   make clean              – remove build artefacts
#
# Installation:
#   make install            – install to ~/.local (user)
#   make install DESTDIR=/  – install to system (requires sudo)
#   sudo make install       – system-wide installation
#
# Packaging:
#   make install PREFIX=/usr DESTDIR=/tmp/pqcheck  – prepare for DEB/RPM
#   fpm -s dir -t deb ...   – build DEB package (see packaging/)
#   fpm -s dir -t rpm ...   – build RPM package (see packaging/)

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2 -g \
           -D_GNU_SOURCE \
           -Isrc
LDFLAGS := -lpcap -lm

# Build with libpq by default. Override with `make WITH_LIBPQ=0` if you don't
# want PostgreSQL client linkage in the default build.
WITH_LIBPQ ?= 1

# Installation paths (FHS compliant)
DESTDIR ?=
PREFIX  ?= $(if $(shell id -u),$(HOME)/.local,/usr/local)
BINDIR  ?= $(PREFIX)/bin
CONFDIR ?= $(PREFIX)/etc/pqcheck
DOCDIR  ?= $(PREFIX)/share/doc/pqcheck
DATADIR ?= $(PREFIX)/share/pqcheck
SYSDIR  ?= $(DESTDIR)/etc/systemd/system

TARGET  := pqCheck
TEST_TARGET := test_detector
TEST_VALID := db_validation_test
VERSION := 1.0.0

SRCS := src/common/util.c \
	src/common/logger.c \
        src/common/net_config.c \
        src/common/pcap_gen.c \
        src/app/cli.c     \
	src/app/cli_help.c \
        src/net/capture.c \
        src/net/reassembly.c \
        src/net/pg_parser.c \
        src/net/packet_parse.c \
        src/analysis/detector.c \
		src/analysis/audit.c \
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

TEST_VALID_SRCS := tests/db_validation_test.c \
				   src/analysis/detector.c \
				   src/analysis/ngram.c   \
				   src/analysis/query_eval.c \
				   src/output/alert.c \
				   src/net/reassembly.c \
				   src/db/db_session.c \
				   src/common/logger.c \
				   src/common/util.c

# --------------------------------------------------------------------------- #
# Optional libpq support                                                      #
# --------------------------------------------------------------------------- #

ifdef WITH_LIBPQ
  CFLAGS  += -DWITH_LIBPQ $(shell pg_config --cppflags 2>/dev/null)
  LDFLAGS += $(shell pg_config --ldflags 2>/dev/null) -lpq
endif

# --------------------------------------------------------------------------- #
# Optional ncurses TUI support                                                #
# --------------------------------------------------------------------------- #

ifdef WITH_TUI
  CFLAGS  += -DWITH_TUI
  LDFLAGS += -lncurses
  SRCS    += src/ui/tui.c
endif

# --------------------------------------------------------------------------- #
# Build rules                                                                 #
# --------------------------------------------------------------------------- #

.PHONY: all test clean install uninstall info help deb rpm audit audit-test
.PHONY: audit-pg audit-ci

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built $@ v$(VERSION)"

$(TEST_TARGET): $(TEST_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


test: $(TEST_TARGET) $(TEST_VALID)
	./$(TEST_TARGET)
	./$(TEST_VALID)
	bash tests/run_rules_compile_test.sh

audit:
	python3 tools/pqcheck_audit.py --root .

audit-pg:
	@echo "Running PostgreSQL system audit using PQ_CONNSTR or env PG* vars"
	@./pqCheck --audit -c "$$PG_CONNSTR"

audit-ci:
	@bash tools/audit_ci.sh

audit-test:
	python3 -m unittest tests/test_pqcheck_audit.py

$(TEST_VALID): $(TEST_VALID_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TEST_TARGET) *.o
	@echo "Cleaned"

install: $(TARGET)
	@mkdir -p $(DESTDIR)$(BINDIR)
	@mkdir -p $(DESTDIR)$(DOCDIR)
	@mkdir -p $(DESTDIR)$(DATADIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -m 644 docs/*.md $(DESTDIR)$(DOCDIR)/
	install -m 644 config/rules.conf $(DESTDIR)$(DATADIR)/rules.conf.example
	install -m 644 config/network.conf $(DESTDIR)$(DATADIR)/network.conf.example
	@if [ "$(DESTDIR)" = "" ] || [ "$(DESTDIR)" = "/" ]; then \
		mkdir -p $(DESTDIR)$(CONFDIR); \
		install -m 644 config/rules.conf $(DESTDIR)$(CONFDIR)/rules.conf; \
		install -m 644 config/network.conf $(DESTDIR)$(CONFDIR)/network.conf.example; \
		mkdir -p $(SYSDIR); \
		install -m 644 packaging/pqcheck.service $(SYSDIR)/pqcheck.service 2>/dev/null || true; \
		echo "[✓] Installed to: $(DESTDIR)$(PREFIX)"; \
		echo "[✓] Binary: $(DESTDIR)$(BINDIR)/$(TARGET)"; \
		echo "[✓] Config: $(DESTDIR)$(CONFDIR)/"; \
		echo "[✓] Docs: $(DESTDIR)$(DOCDIR)/"; \
	fi

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(DOCDIR)
	rm -f $(SYSDIR)/pqcheck.service 2>/dev/null || true
	@echo "Uninstalled from $(DESTDIR)$(PREFIX)"

info:
	@echo "pqCheck v$(VERSION) Build Configuration"
	@echo "========================================"
	@echo "PREFIX:  $(PREFIX)"
	@echo "BINDIR:  $(BINDIR)"
	@echo "CONFDIR: $(CONFDIR)"
	@echo "DOCDIR:  $(DOCDIR)"
	@echo "DATADIR: $(DATADIR)"
	@echo ""
	@echo "Build flags:"
	@echo "  WITH_LIBPQ=$(WITH_LIBPQ) (PostgreSQL correlation)"
	@echo "  WITH_TUI=$(WITH_TUI) (Interactive dashboard)"
	@echo ""
	@echo "Install: make install"
	@echo "Package: make deb OR make rpm"

help:
	@echo "Available targets:"
	@echo "  make              - Build pqCheck"
	@echo "  make WITH_TUI=1   - Build with TUI dashboard"
	@echo "  make WITH_LIBPQ=1 - Build with PostgreSQL support"
	@echo "  make test         - Run tests"
	@echo "  make audit        - Run Python DB security audit"
	@echo "  make audit-pg     - Run native PostgreSQL system audit (requires WITH_LIBPQ build)"
	@echo "  make audit-test   - Run audit CLI unit tests"
	@echo "  make install      - Install locally (~/.local)"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make info         - Show build configuration"
