# code/Makefile — 按特性单独编译 demo，中间产物输出到 out/
#
# 用法：
#   make wq          编译 wq demo
#   make fifo        编译 fifo demo
#   make mem_frag    编译 mem_frag demo
#   make timer       编译 timer demo
#   make event       编译 event demo（epoll + signalfd + timer）
#   make fsm         编译 fsm demo
#   make all         编译全部 demo
#   make clean       清理 out/
#   make help        列出可用 target

ROOT     := .
OUT      := $(ROOT)/out
OBJDIR   := $(OUT)/obj
BINDIR   := $(OUT)/bin

VPATH    := demo:lib/src

CC       := gcc
CPPFLAGS := -I$(ROOT)/lib/include
CFLAGS   := -std=gnu99 -O2 -Wall -Wextra -Werror
LDFLAGS  :=
LDLIBS   :=

# ---------------------------------------------------------------------------
# Demo 注册表（新增 demo：追加到 DEMOS，并定义 <name>_DEMO / <name>_LIB / 可选 FLAGS）
# ---------------------------------------------------------------------------

DEMOS := wq fifo mem_frag timer event crypto kref fsm

wq_DEMO    := wq_demo.c
wq_LIB     := wq.c
wq_LDFLAGS := -pthread

fifo_DEMO    := fifo_demo.c
fifo_LIB     := fifo.c
fifo_LDFLAGS := -pthread

mem_frag_DEMO := mem_frag_demo.c
mem_frag_LIB  := mem_frag_diag.c

timer_DEMO := timer_demo.c
timer_LIB  := timer.c
timer_LDFLAGS := -pthread

event_DEMO := event_demo.c
event_LIB  := timer.c mem_frag_diag.c
event_LDFLAGS := -pthread

crypto_DEMO    := crypto_demo.c
crypto_LIB     := crypto.c
crypto_LDFLAGS := -lcrypto

kref_DEMO    := kref_demo.c
kref_LIB     := refcount.c
kref_LDFLAGS := -pthread

fsm_DEMO := fsm_demo.c
fsm_LIB  := fsm.c

# mem_frag 源文件已在 .c 内 #define _GNU_SOURCE，无需额外 CPPFLAGS

# $(call src_to_obj, wq_demo.c wq.c) -> out/obj/wq_demo.o out/obj/wq.o
src_to_obj = $(addprefix $(OBJDIR)/,$(subst .c,.o,$(notdir $(1))))

# $(call link_demo, wq)
define link_demo
$(1): $(BINDIR)/$(1)

$(BINDIR)/$(1): $(call src_to_obj,$($(1)_DEMO) $($(1)_LIB)) | $(BINDIR)
	$$(CC) $$(LDFLAGS) $$^ -o $$@ $$(LDLIBS) $$($(1)_LDFLAGS)
endef

# ---------------------------------------------------------------------------
# 模式规则：demo/*.c 与 lib/src/*.c 均通过 VPATH 定位
# ---------------------------------------------------------------------------

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------
# 目标
# ---------------------------------------------------------------------------

.PHONY: all clean help $(DEMOS)

all: $(DEMOS)

$(foreach d,$(DEMOS),$(eval $(call link_demo,$(d))))

$(OBJDIR) $(BINDIR):
	mkdir -p $@

help:
	@echo "Usage: make <target>"
	@echo ""
	@echo "Demo targets:"
	@for d in $(DEMOS); do \
		printf "  %-10s -> $(BINDIR)/%s\n" "$$d" "$$d"; \
	done
	@echo ""
	@echo "Other targets:"
	@echo "  all        build all demos"
	@echo "  clean      remove $(OUT)/"
	@echo ""
	@echo "Object files: $(OBJDIR)/"
	@echo "Binaries:     $(BINDIR)/"

clean:
	rm -rf $(OUT)

# 运行示例（可选）：make run-wq
.PHONY: run-wq run-fifo run-mem_frag run-timer run-event run-crypto run-fsm
run-wq: wq
	$(BINDIR)/wq

run-fifo: fifo
	$(BINDIR)/fifo

run-mem_frag: mem_frag
	$(BINDIR)/mem_frag

run-timer: timer
	$(BINDIR)/timer

run-event: event
	$(BINDIR)/event

run-crypto: crypto
	$(BINDIR)/crypto

run-fsm: fsm
	$(BINDIR)/fsm
