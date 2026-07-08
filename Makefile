#
# E2 IoT Device Platform - Phase 1: echo server
#
CC       := gcc
CFLAGS   := -Wall -Wextra -Werror -O2 -std=c11 -D_GNU_SOURCE
CFLAGS   += -I./include -I./include/common -I./include/server
LDFLAGS  := -lpthread

SRCDIR   := src
BUILDDIR := build
TARGET   := $(BUILDDIR)/iot-broker

SRC      := $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*/*.c)
OBJ      := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC))

.PHONY: all clean dirs run bench test_topic test_pub_sub test_integration

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILDDIR)/server $(BUILDDIR)/common

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo ">>> built: $(TARGET)"

clean:
	rm -rf $(BUILDDIR)

run: all
	./$(TARGET) --port 1883 --workers 4

bench: all
	@echo ">>> starting server in background..."
	@./$(TARGET) --port 1883 --workers 4 & echo $$! > /tmp/iot-broker.pid
	@sleep 1
	@echo ">>> running 500-connection bench..."
	@python3 tools/bench.py --host 127.0.0.1 --port 1883 --clients 500
	@echo ">>> stopping server..."
	@kill $$(cat /tmp/iot-broker.pid) 2>/dev/null; rm -f /tmp/iot-broker.pid

test_topic: all
	@echo ">>> running topic matching unit tests..."
	@python3 tools/test_topic.py

test_integration: all
	@echo ">>> running MQTT integration tests (Phase 2)..."
	@python3 tools/test_mqtt_integration.py
	@echo ">>> running PUB/SUB integration tests (Phase 3)..."
	@python3 tools/test_pub_sub.py

test_pub_sub: all
	@echo ">>> running PUB/SUB/UNSUB integration tests..."
	@python3 tools/test_pub_sub.py

test_phase4: all
	@echo ">>> running Phase 4 integration tests (Keepalive/QoS1/Will)..."
	@python3 tools/test_phase4.py
