ifndef FIFTYONEDEGREES_NGINX_VERSION
	VERSION := 1.20.0
else
	VERSION := $(FIFTYONEDEGREES_NGINX_VERSION)
endif
TESTS := tests/51degrees.t


$(eval $(API):;@:)

ifneq (test,$(firstword $(MAKECMDGOALS)))
	API := hash
	ARGS := -std=gnu11 -Wall -Wno-unused-variable -Wno-missing-braces
endif


FULLPATH := $(shell pwd)
ifndef FIFTYONEDEGREES_DATAFILE
	DATAFILE := 51Degrees-LiteV4.1.hash
else
	DATAFILE := $(FIFTYONEDEGREES_DATAFILE)
endif

MODULEPATH := $(FULLPATH)/build/modules/ngx_http_51D_module.so
FILEPATH := $(FULLPATH)/device-detection-cxx/device-detection-data/$(DATAFILE)


ifndef STATIC_BUILD
	MODULE_ARG := --add-dynamic-module
else
	MODULE_ARG := --add-module
endif

.PHONY hash:

clean:
	if [ -d "51Degrees_module/src" ]; then \
		rm -rf 51Degrees_module/src; \
	fi
	if [ -d "build" ]; then \
		rm -rf build; \
	fi
	if [ -f "nginx" ]; then \
		rm -f nginx; \
	fi
	if [ -f "vendor/nginx-$(VERSION)/Makefile" ]; then \
		cd $(CURDIR)/vendor/nginx-$(VERSION) && make clean; \
	fi
	if [ -d "tests/nginx-tests" ]; then \
		rm -r tests/nginx-tests; \
	fi;

build: clean
	mkdir -p 51Degrees_module/src/hash
	mkdir -p 51Degrees_module/src/common-cxx
	exit
	cp module_conf/$(API)_config 51Degrees_module/config
	cp device-detection-cxx/src/*.c 51Degrees_module/src/
	cp device-detection-cxx/src/*.h 51Degrees_module/src/
	cp device-detection-cxx/src/hash/*.c 51Degrees_module/src/hash/
	cp device-detection-cxx/src/hash/*.h 51Degrees_module/src/hash/
	cp device-detection-cxx/src/common-cxx/*.c 51Degrees_module/src/common-cxx/
	cp device-detection-cxx/src/common-cxx/*.h 51Degrees_module/src/common-cxx/


get-source:
	if [ ! -d "vendor" ]; then mkdir vendor; fi
	cd vendor && curl -L -O "http://nginx.org/download/nginx-$(VERSION).tar.gz"
	cd vendor && tar xzf "nginx-$(VERSION).tar.gz"

configure: build
	if [ ! -d "vendor/nginx-$(VERSION)" ]; then $(MAKE) get-source; fi
	cd $(CURDIR)/vendor/nginx-$(VERSION) && \
	./configure \
	--prefix=$(CURDIR)/build \
	--with-ld-opt="-lm -latomic $(MEM_LD_FLAGS)" \
	$(MODULE_ARG)=$(CURDIR)/51Degrees_module \
	--with-compat \
	--with-cc-opt="$(ARGS) $(MEM_CC_FLAGS)" \
	--with-debug \
	--sbin-path=$(CURDIR) \
	--conf-path="nginx.conf" \
	--with-http_sub_module

install: configure
	cd $(CURDIR)/vendor/nginx-$(VERSION) && make install
	sed "/\/\*\*/,/\*\//d" examples/$(API)/gettingStarted.conf > build/nginx.conf
	sed -i "s!%%DAEMON_MODE%%!on!g" build/nginx.conf
	sed -i "s!%%MODULE_PATH%%!!g" build/nginx.conf
	sed -i "s!%%FILE_PATH%%!$(FILEPATH)!g" build/nginx.conf
	sed -i "s!%%TEST_GLOBALS%%!!g" build/nginx.conf
	sed -i "s!%%TEST_GLOBALS_HTTP%%!!g" build/nginx.conf
	echo > build/html/$(API)
	

module: configure
	cd $(CURDIR)/vendor/nginx-$(VERSION) && make modules
	if [ ! -d "build/modules" ]; then mkdir -p build/modules; fi
	cp $(CURDIR)/vendor/nginx-$(VERSION)/objs/*.so build/modules

all-versions:
	mkdir modules
	$(foreach version, \
		1.19.0 1.19.5 1.19.8 1.20.0, \
		$(MAKE) module VERSION=$(version); \
		mv build/modules/ngx_http_51D_module.so modules/ngx_http_51D_hash_module-$(version).so;)

set-mem:
	$(eval MEM_CC_FLAGS := -O0 -g -fsanitize=address)
	$(eval MEM_LD_FLAGS := -g -fsanitize=address)

mem-check: set-mem install
	
test-prep:
	if [ ! -f "tests/nginx-tests/lib/Test/Nginx.pm" ]; then \
	rm -r tests/nginx-tests*; \
	cd tests && curl -L -O "http://hg.nginx.org/nginx-tests/archive/tip.tar.gz"; \
	tar zxf tip.tar.gz; \
	mv nginx-tests* nginx-tests; \
	fi;

test-full:
	$(MAKE) test TESTS="$(TESTS) tests/nginx-tests"

test-examples: test-prep
ifeq (,$(wildcard $(FULLPATH)/nginx))
	$(error Local binary must be built first (use "make install"))
else
	$(eval CMD := TEST_NGINX_BINARY="$(FULLPATH)/nginx" TEST_MODULE_PATH="$(FULLPATH)/build/" TEST_FILE_PATH="$(FILEPATH)" prove $(FIFTYONEDEGREES_FORMATTER) -v tests/examples :: $(DATAFILE))
ifdef FIFTYONEDEGREES_TEST_OUTPUT
	$(CMD) > $(FIFTYONEDEGREES_TEST_OUTPUT)
else
	$(CMD)
endif
endif

test: test-prep
ifeq (,$(wildcard $(FULLPATH)/nginx))
	$(error Local binary must be built first (use "make install"))
else
	$(eval CMD := TEST_NGINX_BINARY="$(FULLPATH)/nginx" TEST_NGINX_GLOBALS="load_module $(MODULEPATH);" TEST_NGINX_GLOBALS_HTTP="51D_file_path $(FILEPATH);" prove $(FIFTYONEDEGREES_FORMATTER) -v $(TESTS) :: $(DATAFILE))
ifdef FIFTYONEDEGREES_TEST_OUTPUT
	$(CMD) > $(FIFTYONEDEGREES_TEST_OUTPUT) 2>&1
else
	$(CMD)
endif
endif
