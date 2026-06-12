ifndef FIFTYONEDEGREES_NGINX_VERSION
	VERSION := 1.20.0
else
	VERSION := $(FIFTYONEDEGREES_NGINX_VERSION)
endif

ifndef DONT_CLEAN_TESTS
	CLEANTESTS := 1
else
	CLEANTESTS := 0
endif

TESTS := tests/51degrees.t


$(eval $(API):;@:)

ifneq (test,$(firstword $(MAKECMDGOALS)))
	API := hash
	ARGS := -std=gnu11 -Wall -Wno-unused-variable -Wno-missing-braces -Wno-type-limits -Wno-unused-function -Wno-missing-field-initializers -Wno-unused-but-set-variable
endif


FULLPATH := $(shell pwd)
ifndef FIFTYONEDEGREES_DATAFILE
	DATAFILE := 51Degrees-LiteV4.1.hash
else
$(warning FIFTYONEDEGREES_DATAFILE is deprecated and will be removed in a future release. Use 51DEGREES_DD_PATH with an explicit path to the data file instead.)
	DATAFILE := $(FIFTYONEDEGREES_DATAFILE)
endif

ifndef FIFTYONEDEGREES_DATAFILE_IPI
	DATAFILE_IPI := 51Degrees-IPIV4AsnIpiV41.ipi
else
$(warning FIFTYONEDEGREES_DATAFILE_IPI is deprecated and will be removed in a future release. Use 51DEGREES_IPI_PATH with an explicit path to the data file instead.)
	DATAFILE_IPI := $(FIFTYONEDEGREES_DATAFILE_IPI)
endif

MODULEPATH := $(FULLPATH)/build/modules/ngx_http_51D_module.so
FILEPATH := $(FULLPATH)/device-detection-cxx/device-detection-data/$(DATAFILE)
FILEPATH_IPI := $(FULLPATH)/ip-intelligence-cxx/ip-intelligence-data/$(DATAFILE_IPI)

# The aligned environment variable 51DEGREES_DD_PATH provides an explicit
# path to a device detection data file and is checked first. The legacy
# FIFTYONEDEGREES_DATAFILE name variable above is retained for backwards
# compatibility and is combined with the expected data folder when no
# explicit path is provided.
ifdef 51DEGREES_DD_PATH
	FILEPATH := $(51DEGREES_DD_PATH)
	DATAFILE := $(notdir $(51DEGREES_DD_PATH))
endif

# The aligned environment variable 51DEGREES_IPI_PATH provides an explicit
# path to an IP intelligence data file in the same way.
ifdef 51DEGREES_IPI_PATH
	FILEPATH_IPI := $(51DEGREES_IPI_PATH)
	DATAFILE_IPI := $(notdir $(51DEGREES_IPI_PATH))
endif


# For dynamic builds, both the device detection and IP intelligence modules
# are built. For static builds, only the module selected by the API variable
# (hash by default) is linked in. The two modules cannot be linked statically
# into one binary as each carries its own copy of the common-cxx sources
# compiled with different data file layout options.
ifndef STATIC_BUILD
	MODULE_ARGS := --add-dynamic-module=$(CURDIR)/51Degrees_hash_module --add-dynamic-module=$(CURDIR)/51Degrees_ipi_module
else
	ifeq ($(API),ipi)
		MODULE_ARGS := --add-module=$(CURDIR)/51Degrees_ipi_module
	else
		MODULE_ARGS := --add-module=$(CURDIR)/51Degrees_hash_module
	endif
endif

.PHONY hash:

clean:
	if [ -d "51Degrees_hash_module/src" ]; then \
		rm -rf 51Degrees_hash_module/src; \
	fi
	if [ -d "51Degrees_ipi_module/src" ]; then \
		rm -rf 51Degrees_ipi_module/src; \
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
	if [ "$(CLEANTESTS)" -eq "1" ]; then \
		if [ -d "tests/nginx-tests" ]; then \
			rm -r tests/nginx-tests; \
		fi; \
	fi;

build: clean
	mkdir -p 51Degrees_hash_module/src/hash
	mkdir -p 51Degrees_hash_module/src/common-cxx
	# exit
	cp module_conf/hash_config 51Degrees_hash_module/config
	cp device-detection-cxx/src/*.c 51Degrees_hash_module/src/
	cp device-detection-cxx/src/*.h 51Degrees_hash_module/src/
	cp device-detection-cxx/src/hash/*.c 51Degrees_hash_module/src/hash/
	cp device-detection-cxx/src/hash/*.h 51Degrees_hash_module/src/hash/
	cp device-detection-cxx/src/common-cxx/*.c 51Degrees_hash_module/src/common-cxx/
	cp device-detection-cxx/src/common-cxx/*.h 51Degrees_hash_module/src/common-cxx/
	# The IP intelligence module carries its own copy of the common-cxx
	# sources as the IP intelligence data file format requires them to be
	# compiled with different data file layout options to device detection.
	# The directories are renamed (and the include paths rewritten below)
	# because nginx names addon object files by the last directory component
	# only, so directory names must be unique across the two modules.
	mkdir -p 51Degrees_ipi_module/src/ipi-common-cxx
	mkdir -p 51Degrees_ipi_module/src/ipi-graph
	cp module_conf/ipi_config 51Degrees_ipi_module/config
	cp ip-intelligence-cxx/src/*.c 51Degrees_ipi_module/src/
	cp ip-intelligence-cxx/src/*.h 51Degrees_ipi_module/src/
	cp ip-intelligence-cxx/src/ip-graph-cxx/*.c 51Degrees_ipi_module/src/ipi-graph/
	cp ip-intelligence-cxx/src/ip-graph-cxx/*.h 51Degrees_ipi_module/src/ipi-graph/
	cp ip-intelligence-cxx/src/common-cxx/*.c 51Degrees_ipi_module/src/ipi-common-cxx/
	cp ip-intelligence-cxx/src/common-cxx/*.h 51Degrees_ipi_module/src/ipi-common-cxx/
	# Rewrite the include paths in the copied sources to match the renamed
	# directories.
	find 51Degrees_ipi_module/src -maxdepth 1 \( -name "*.c" -o -name "*.h" \) -exec sed -i \
		-e 's#"common-cxx/#"ipi-common-cxx/#g' \
		-e 's#"ip-graph-cxx/#"ipi-graph/#g' {} +
	find 51Degrees_ipi_module/src/ipi-graph \( -name "*.c" -o -name "*.h" \) -exec sed -i \
		-e 's#"\.\./common-cxx/#"../ipi-common-cxx/#g' {} +
	# Inject the data file layout defines into the copied IP intelligence
	# sources. Nginx does not support per module compiler flags so the
	# defines cannot be set globally without affecting the device detection
	# module. These must match the defines in ngx_http_51D_ipi_module.c.
	find 51Degrees_ipi_module/src -name "*.c" -exec sed -i \
		-e '1i #define FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT' \
		-e '1i #define FIFTYONE_DEGREES_REDUCED_FILE' \
		-e '1i #define _FILE_OFFSET_BITS 64' {} +


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
	$(MODULE_ARGS) \
	--with-compat \
	--with-cc-opt="$(ARGS) $(MEM_CC_FLAGS)" \
	--with-debug \
	--sbin-path=$(CURDIR) \
	--conf-path="nginx.conf" \
	--with-http_sub_module

configure-no-module: clean
	if [ ! -d "vendor/nginx-$(VERSION)" ]; then $(MAKE) get-source; fi
	cd $(CURDIR)/vendor/nginx-$(VERSION) && \
	./configure \
	--prefix=$(CURDIR)/build \
	--with-compat \
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
	sed -i "s!%%FILE_PATH_IPI%%!$(FILEPATH_IPI)!g" build/nginx.conf
	sed -i "s!%%TEST_GLOBALS%%!!g" build/nginx.conf
	sed -i "s!%%TEST_GLOBALS_HTTP%%!!g" build/nginx.conf
	echo > build/html/$(API)

install-no-module: configure-no-module	
	cd $(CURDIR)/vendor/nginx-$(VERSION) && make install

module: configure
	cd $(CURDIR)/vendor/nginx-$(VERSION) && make modules
	if [ ! -d "build/modules" ]; then mkdir -p build/modules; fi
	cp $(CURDIR)/vendor/nginx-$(VERSION)/objs/*.so build/modules

all-versions:
	mkdir modules
	$(foreach version, \
		1.19.0 1.19.5 1.19.8 1.20.0, \
		$(MAKE) module VERSION=$(version); \
		mv build/modules/ngx_http_51D_module.so modules/ngx_http_51D_hash_module-$(version).so; \
		mv build/modules/ngx_http_51D_ipi_module.so modules/ngx_http_51D_ipi_module-$(version).so;)

set-mem:
	$(eval MEM_CC_FLAGS := -O0 -g -fsanitize=address)
	$(eval MEM_LD_FLAGS := -g -fsanitize=address)

mem-check: set-mem install
	
test-prep:
	if [ ! -f "tests/nginx-tests/lib/Test/Nginx.pm" ]; then \
		rm -rf tests/nginx-tests; mkdir -p tests/nginx-tests; \
		curl -fL "https://github.com/nginx/nginx-tests/archive/refs/heads/master.tar.gz" \
		| tar -xzC tests/nginx-tests --strip-components 1; \
	fi;

test-full:
	$(MAKE) test TESTS="$(TESTS) tests/nginx-tests"

test-examples: test-prep
ifeq (,$(wildcard $(FULLPATH)/nginx))
	$(error Local binary must be built first (use "make install"))
else
	$(eval CMD := TEST_NGINX_BINARY="$(FULLPATH)/nginx" TEST_MODULE_PATH="$(FULLPATH)/build/" TEST_FILE_PATH="$(FILEPATH)" TEST_FILE_PATH_IPI="$(FILEPATH_IPI)" ASAN_OPTIONS=detect_odr_violation=0 LSAN_OPTIONS=suppressions=suppressions.txt prove $(FIFTYONEDEGREES_FORMATTER) -v tests/examples :: $(DATAFILE))
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
	$(eval CMD := TEST_NGINX_BINARY="$(FULLPATH)/nginx" TEST_NGINX_GLOBALS="load_module $(MODULEPATH);" TEST_NGINX_GLOBALS_HTTP="51D_file_path $(FILEPATH);" ASAN_OPTIONS=detect_odr_violation=0 LSAN_OPTIONS=suppressions=suppressions.txt prove $(FIFTYONEDEGREES_FORMATTER) -v $(TESTS) :: $(DATAFILE))
ifdef FIFTYONEDEGREES_TEST_OUTPUT
	$(CMD) > $(FIFTYONEDEGREES_TEST_OUTPUT)
else
	$(CMD)
endif
endif
