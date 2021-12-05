SRCS= \
iso14229.c \
isotp/isotp.c

HDRS= \
iso14229.h \
isotp/isotp.h \
isotp/isotp_config.h \
isotp/isotp_defines.h \
isotp/isotp_user.h

INCLUDES= \
isotp

py_requirements: requirements.txt
	pip3 install -r requirements.txt

# Tests (Run locally on linux)
DEFINES=\
ISO14229USERDEBUG=printf

TEST_CFLAGS += $(foreach i,$(INCLUDES),-I$(i))
TEST_CFLAGS += $(foreach d,$(DEFINES),-D$(d))

TEST_SRCS= \
test_iso14229_harness.c

TEST_CFLAGS += -g -shared -fPIC

test_iso14229_harness.so: $(TEST_SRCS) $(SRCS) $(HDRS) Makefile
	$(CC) $(TEST_CFLAGS) $(TEST_SRCS) $(SRCS) -o $@

test: test_iso14229_harness.so py_requirements
	pytest

test_interactive:
	gdb -x .tests.gdbinit

clean:
	rm -rf test_iso14229_harness.so


# Example

EXAMPLE_SRCS=\
example/simple.c \
example/linux_host.c

EXAMPLE_HDRS=\
example/simple.h

EXAMPLE_INCLUDES=\
example

EXAMPLE_CFLAGS += $(foreach i,$(INCLUDES),-I$(i))
EXAMPLE_CFLAGS += $(foreach i,$(EXAMPLE_INCLUDES),-I$(i))
EXAMPLE_CFLAGS += $(foreach d,$(DEFINES),-D$(d))
EXAMPLE_CFLAGS += -g 

example/linux: $(SRCS) $(EXAMPLE_SRCS) $(HDRS) $(EXAMPLE_HDRS) Makefile
	$(CC) $(EXAMPLE_CFLAGS) -o $@ $(EXAMPLE_SRCS) $(SRCS) 

.phony: py_requirements
