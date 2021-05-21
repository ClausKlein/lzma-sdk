.PHONY: all clean

all: build
	$(MAKE) -C Util/Lzma
	ninja -C $<
	ninja -C $< test

check: build
	run-clang-tidy.py -p $< -checks='-readability-*,-cppcoreguidelines-init-variables,-cert-dcl37-c,-cert-dcl51-cpp'

build:
	cmake -G Ninja -B $@ -S .

clean:
	$(MAKE) -C Util/Lzma $@
	rm -rf build tags
	find . -name '*~' -delete
