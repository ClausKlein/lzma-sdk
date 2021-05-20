.PHONY: all clean

all: build
	ninja -C $<
	# ninja -C $< test
	run-clang-tidy.py -p $< -checks='-readability-*,-cppcoreguidelines-init-variables,-cert-dcl37-c,-cert-dcl51-cpp'

build:
	cmake -G Ninja -B $@ -S .

clean:
	rm -rf build tags
	find . -name '*~' -delete
