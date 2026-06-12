/* *********************************************************************
 * This Original Work is copyright of 51 Degrees Mobile Experts Limited.
 * Copyright 2026 51 Degrees Mobile Experts Limited, Davidson House,
 * Forbury Square, Reading, Berkshire, United Kingdom RG1 3EU.
 *
 * This Original Work is licensed under the European Union Public Licence
 * (EUPL) v.1.2 and is subject to its terms as set out below.
 *
 * If a copy of the EUPL was not distributed with this file, You can obtain
 * one at https://opensource.org/licenses/EUPL-1.2.
 *
 * The 'Compatible Licences' set out in the Appendix to the EUPL (as may be
 * amended by the European Commission) shall be deemed incompatible for
 * the purposes of the Work and the provisions of the compatibility
 * clause in Article 5 of the EUPL shall not apply.
 *
 * If using the Work as, or as part of, a network application, by
 * including the attribution notice(s) required under Article 5 of the EUPL
 * in the end user terms of the application under an appropriate heading,
 * such notice(s) shall fulfill the requirements of that article.
 * ********************************************************************* */

/*
 * The IP intelligence data file format uses 64 bit collection offsets and
 * the reduced profile and value structures. These macros must match the
 * ones used to compile the engine sources (see module_conf/ipi_config and
 * the build target in the Makefile) as they change the layout of the
 * structures shared with the engine.
 */
#define FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT
#define FIFTYONE_DEGREES_REDUCED_FILE
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_string.h>
#include <inttypes.h>
#include "src/ipi.h"
#undef MAP_TYPE
#include "src/fiftyone.h"

/**
 * @defgroup ngx_http_51D_ipi_module 51Degrees IP Intelligence Nginx Module
 * Internals
 *
 * @{
 */

#ifndef FIFTYONE_DEGREES_IPI_PROPERTY_NOT_AVAILABLE
/**
 * Default property value if it is not available or no match is found.
 */
#define FIFTYONE_DEGREES_IPI_PROPERTY_NOT_AVAILABLE "NoMatch"
#endif

#ifndef FIFTYONE_DEGREES_IPI_VALUE_SEPARATOR
/**
 * Default separator.
 */
#define FIFTYONE_DEGREES_IPI_VALUE_SEPARATOR (u_char*) ","
#endif

#ifndef FIFTYONE_DEGREES_IPI_MAX_STRING
/**
 * Maximum value string being returned. Some properties such as Areas
 * return large WKT strings, so allow a reasonably large buffer.
 */
#define FIFTYONE_DEGREES_IPI_MAX_STRING 20000
#endif

#ifndef FIFTYONE_DEGREES_IPI_MAX_PROPS_STRING
/**
 * Maximum size of string holding the full properties list.
 */
#define FIFTYONE_DEGREES_IPI_MAX_PROPS_STRING 2048
#endif

/**
 * NGINX shared memory zone requires minimum of 8 bytes and allocated size
 * has to be power of 2. Any non-conforming value will be rounded up. Thus
 * make this adjustment value account for the extra rounded up memory.
 */
#define FIFTYONE_DEGREES_IPI_MEMORY_ADJUSTMENT 1.1

/**
 * 3 Config levels main, server and location.
 */
#define FIFTYONE_DEGREES_IPI_CONFIG_LEVELS 3

/**
 * Global module declaration.
 */
ngx_module_t ngx_http_51D_ipi_module;

// Configuration function declarations.
static void *ngx_http_51D_ipi_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_51D_ipi_create_srv_conf(ngx_conf_t *cf);
static void *ngx_http_51D_ipi_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_51D_ipi_merge_loc_conf(
	ngx_conf_t *cf, void *parent, void *child);
static char *ngx_http_51D_ipi_merge_srv_conf(
	ngx_conf_t *cf, void *parent, void *child);

// Set function declarations.
static char *ngx_http_51D_ipi_set_loc(
	ngx_conf_t* cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_51D_ipi_set_srv(
	ngx_conf_t* cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_51D_ipi_set_main(
	ngx_conf_t* cf, ngx_command_t *cmd, void *conf);

// Request handler declaration.
static ngx_int_t ngx_http_51D_ipi_handler(ngx_http_request_t *r);

/**
 * Pointer to the shared memory zone for the resource manager.
 */
static ngx_shm_zone_t *ngx_http_51D_ipi_shm_resource_manager;
/**
 * Forward declaration of #ngx_http_51D_ipi_init_shm_resource_manager
 */
static ngx_int_t ngx_http_51D_ipi_init_shm_resource_manager(
	ngx_shm_zone_t *shm_zone, void *data);
/**
 * Atomic integer used to ensure a new data set memory zone on each reload.
 */
static ngx_atomic_t ngx_http_51D_ipi_shm_tag = 1;
/**
 * Count number of current worker processes.
 */
static ngx_atomic_t *ngx_http_51D_ipi_worker_count;

/**
 * Structure containing details of a specific header to be set as per the
 * config file.
 */
typedef struct ngx_http_51D_ipi_data_to_set_t ngx_http_51D_ipi_data_to_set;

struct ngx_http_51D_ipi_data_to_set_t {
	ngx_uint_t propertyCount;           /**< The number of properties in the
	                                         property array. */
	ngx_str_t **property;               /**< Array of properties to set. */
	ngx_str_t headerName;               /**< The header name to set. */
	ngx_str_t lowerHeaderName;          /**< The header name in lower case. */
	ngx_str_t variableName;             /**< The name of the variable to use
	                                         in place of the client IP
	                                         address. */
	ngx_http_51D_ipi_data_to_set *next; /**< The next header in the list. */
};

/**
 * Match config structure set from the config file.
 */
typedef struct {
	ngx_uint_t headerCount;              /**< The number of headers to set. */
	ngx_http_51D_ipi_data_to_set *header; /**< List of headers to set. */
} ngx_http_51D_ipi_match_conf_t;

/**
 * Module location config.
 */
typedef struct {
	ngx_http_51D_ipi_match_conf_t matchConf; /**< The match to carry out in
	                                              this location. */
} ngx_http_51D_ipi_loc_conf_t;

/**
 * Module main config.
 */
typedef struct {
	char properties[FIFTYONE_DEGREES_IPI_MAX_PROPS_STRING]; /**< Properties
	                                                   string to initialise
	                                                   the data set with. */
	ngx_str_t dataFile;                /**< 51Degrees IP intelligence data
	                                        file. */
	ResultsIpi *results;               /**< 51Degrees results, local to each
	                                        process. */
	char valueString[FIFTYONE_DEGREES_IPI_MAX_STRING]; /**< Result string
	                                        buffer, local to each process. */
	ResourceManager *resourceManager;  /**< 51Degrees data set, shared across
	                                        all process'. */
	ngx_str_t valueSeparator;          /**< Match header value separator. */
	ngx_uint_t maxConcurrency;         /**< 51Degrees max concurrency value
	                                        to set. */
	ngx_http_51D_ipi_match_conf_t matchConf; /**< The match to carry out in
	                                              this block's locations. */
} ngx_http_51D_ipi_main_conf_t;

/**
 * Module server config.
 */
typedef struct {
	ngx_http_51D_ipi_match_conf_t matchConf; /**< The match to carry out in
	                                              this server's locations. */
} ngx_http_51D_ipi_srv_conf_t;

/**
 * Report the status code returned by one of the 51Degrees APIs.
 * @param log the log to write the error message to.
 * @param status the status code returned by one of the 51Degrees APIs.
 * @param fileName of the data file being used.
 * @return error
 */
static ngx_int_t
report_status(
	ngx_log_t *log,
	StatusCode status,
	const char *fileName) {
	const char *fodMessage = StatusGetMessage(status, fileName);
	if (fodMessage != NULL) {
		char *message =
			(char *)ngx_alloc(
				ngx_strlen("51Degrees ") + ngx_strlen(fodMessage) + 1, log);
		if (message != NULL) {
			if (sprintf(message, "51Degrees %s", fodMessage) > 0) {
				ngx_log_error(NGX_LOG_ERR, log, 0, message);
			}
			else {
				ngx_log_error(
					NGX_LOG_ERR,
					log,
					0,
					"51Degrees failed to construct error message.");
			}
			ngx_free(message);
		}
		else {
			ngx_log_error(
				NGX_LOG_ERR,
				log,
				0,
				"51Degrees failed to allocate memory for error message.");
		}
		free((char *)fodMessage);
	}
	return NGX_ERROR;
}

/**
 * Report the insufficient memory since memory allocation is a common
 * activity.
 * @param log the log to write the message to.
 * @return error
 */
static ngx_int_t
report_insufficient_memory_status(
	ngx_log_t *log) {
	return report_status(
		log,
		FIFTYONE_DEGREES_STATUS_INSUFFICIENT_MEMORY,
		"");
}

/**
 * Get a fiftyoneDegreesConfigIpi instance based on the main configuration.
 * Only the in memory performance profile is supported as the data set is
 * held in shared memory.
 * @param fdmcf main configuration
 * @return fiftyoneDegreesConfigIpi instance
 */
static ConfigIpi
get_config_ipi(ngx_http_51D_ipi_main_conf_t *fdmcf) {
	ConfigIpi config = IpiInMemoryConfig;

	// Max concurrency is always set to the number of worker processes
	if (fdmcf->maxConcurrency != NGX_CONF_UNSET_UINT) {
		config.strings.concurrency = fdmcf->maxConcurrency;
		config.components.concurrency = fdmcf->maxConcurrency;
		config.maps.concurrency = fdmcf->maxConcurrency;
		config.properties.concurrency = fdmcf->maxConcurrency;
		config.values.concurrency = fdmcf->maxConcurrency;
		config.profiles.concurrency = fdmcf->maxConcurrency;
		config.graphs.concurrency = fdmcf->maxConcurrency;
		config.profileGroups.concurrency = fdmcf->maxConcurrency;
		config.profileOffsets.concurrency = fdmcf->maxConcurrency;
		config.propertyTypes.concurrency = fdmcf->maxConcurrency;
		config.graph.concurrency = fdmcf->maxConcurrency;
	}

	return config;
}

/**
 * Get the required properties to initialise the engine with. Where one or
 * more 51D_match_ipi directives are present, only the properties they
 * reference are required. Otherwise all properties in the data file are
 * used.
 * @param fdmcf main configuration
 * @return fiftyoneDegreesPropertiesRequired instance
 */
static PropertiesRequired
get_properties_ipi(ngx_http_51D_ipi_main_conf_t *fdmcf) {
	PropertiesRequired properties = PropertiesDefault;
	if (fdmcf->properties[0] != '\0') {
		properties.string = fdmcf->properties;
	}
	return properties;
}

/**
 * Module post config. Adds the module to the HTTP rewrite phase array, sets
 * the defaults if necessary, and sets the shared memory zone used to hold
 * the resource manager and data set.
 * @param cf nginx config.
 * @return ngx_int_t nginx conf status.
 */
static ngx_int_t
ngx_http_51D_ipi_post_conf(ngx_conf_t *cf)
{
	ngx_http_handler_pt *h;
	ngx_http_core_main_conf_t *cmcf;
	ngx_http_51D_ipi_main_conf_t *fdmcf;
	ngx_atomic_int_t tagOffset;
	ngx_str_t resourceManagerName;
	size_t size;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	fdmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_51D_ipi_module);

	// set a handler at rewrite phase to perform matching
	h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
	if (h == NULL) {
		ngx_conf_log_error(
			NGX_LOG_ERR,
			cf,
			0,
			"51Degrees failed to get a handler entry.");
		return NGX_ERROR;
	}

	*h = ngx_http_51D_ipi_handler;

	// Set the default value separator if necessary.
	if ((int)fdmcf->valueSeparator.len <= 0) {
		fdmcf->valueSeparator.data = FIFTYONE_DEGREES_IPI_VALUE_SEPARATOR;
		fdmcf->valueSeparator.len = ngx_strlen(fdmcf->valueSeparator.data);
	}

	// Check if a data file was set.
	if ((int)fdmcf->dataFile.len <= 0) {
		ngx_conf_log_error(
			NGX_LOG_NOTICE,
			cf,
			0,
			"51Degrees no IP intelligence data file was set.");
		ngx_log_error(
			NGX_LOG_INFO,
			cf->cycle->log,
			0,
			"51D_file_path_ipi was not set. No resource was loaded.");
		return NGX_OK;
	}

	// Initialise the shared memory zone for the resource manager.
	resourceManagerName.data =
		(u_char *) "51Degrees Shared Resource Manager IPI";
	resourceManagerName.len = ngx_strlen(resourceManagerName.data);
	// By increasing the tag each time, the shared memory zone won't be
	// reused. Thus, during a reload, the old allocated resources in the
	// shared memory will be freed automatically.
	tagOffset =
		ngx_atomic_fetch_add(&ngx_http_51D_ipi_shm_tag, (ngx_atomic_int_t)1);

	// Need to get the size of memory that the resource manager will occupy.
	ConfigIpi config = get_config_ipi(fdmcf);
	PropertiesRequired properties = get_properties_ipi(fdmcf);

	EXCEPTION_CREATE
	size = fiftyoneDegreesIpiSizeManagerFromFile(
		&config,
		&properties,
		(const char *)fdmcf->dataFile.data,
		exception);
	if ((int)size < 1 || EXCEPTION_FAILED) {
		// If there was a problem, throw an error.
		return report_status(
			cf->cycle->log,
			exception->status,
			(const char *)fdmcf->dataFile.data);
	}
	// Add the size of the resource manager and worker count
	size += sizeof(ResourceManager) + sizeof(ngx_atomic_t);

	// The data is held in shared memory where each object is required
	// to be power of 2 and minimum of 8 bytes. Non-conforming value
	// will be rounded up. Thus, adjust the size to cope with cases
	// where allocated size is rounded up.
	size *= FIFTYONE_DEGREES_IPI_MEMORY_ADJUSTMENT;

	ngx_http_51D_ipi_shm_resource_manager =
		ngx_shared_memory_add(
			cf,
			&resourceManagerName,
			size,
			&ngx_http_51D_ipi_module + tagOffset);
	ngx_http_51D_ipi_shm_resource_manager->init =
		ngx_http_51D_ipi_init_shm_resource_manager;
	return NGX_OK;
}

/**
 * Initialises the match config so no headers are set.
 * @param matchConf to initialise.
 */
static void
ngx_http_51D_ipi_init_match_conf(ngx_http_51D_ipi_match_conf_t *matchConf)
{
	matchConf->headerCount = 0;
	matchConf->header = NULL;
}

/**
 * Create main config. Allocates memory to the configuration and initialises
 * config options to -1 (unset).
 * @param cf nginx config.
 * @return Pointer to module main config.
 */
static void *
ngx_http_51D_ipi_create_main_conf(ngx_conf_t *cf)
{
	ngx_http_51D_ipi_main_conf_t *conf;
	ngx_core_conf_t *ccf =
		(ngx_core_conf_t *)ngx_get_conf(cf->cycle->conf_ctx, ngx_core_module);

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_51D_ipi_main_conf_t));
	if (conf == NULL) {
		return NULL;
	}

	conf->maxConcurrency = ccf->worker_processes;

	memset(conf->properties, 0, FIFTYONE_DEGREES_IPI_MAX_PROPS_STRING);
	conf->dataFile = (ngx_str_t)ngx_null_string;
	conf->results = NULL;
	memset(conf->valueString, 0, FIFTYONE_DEGREES_IPI_MAX_STRING);
	conf->resourceManager = NULL;
	conf->valueSeparator = (ngx_str_t)ngx_null_string;

	ngx_http_51D_ipi_init_match_conf(&conf->matchConf);
	return conf;
}

/**
 * Create location config. Allocates memory to the configuration.
 * @param cf nginx config.
 * @return Pointer to module location config.
 */
static void *
ngx_http_51D_ipi_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_51D_ipi_loc_conf_t *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_51D_ipi_loc_conf_t));
	if (conf == NULL) {
		return NULL;
	}
	ngx_http_51D_ipi_init_match_conf(&conf->matchConf);
	return conf;
}

/**
 * Create server config. Allocates memory to the configuration.
 * @param cf nginx config.
 * @return Pointer to the server config.
 */
static void *
ngx_http_51D_ipi_create_srv_conf(ngx_conf_t *cf)
{
	ngx_http_51D_ipi_srv_conf_t *conf;

	conf = ngx_palloc(cf->pool, sizeof(ngx_http_51D_ipi_srv_conf_t));
	if (conf == NULL) {
		return NULL;
	}
	ngx_http_51D_ipi_init_match_conf(&conf->matchConf);
	return conf;
}

/**
 * Merge location config. Either gets the value of count that is set, or
 * sets to the default of 0.
 */
static char *
ngx_http_51D_ipi_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_51D_ipi_loc_conf_t *prev = parent;
	ngx_http_51D_ipi_loc_conf_t *conf = child;

	ngx_conf_merge_uint_value(
		conf->matchConf.headerCount, prev->matchConf.headerCount, 0);

	return NGX_CONF_OK;
}

/**
 * Merge server config. Either gets the value of count that is set, or
 * sets to the default of 0.
 */
static char *
ngx_http_51D_ipi_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_51D_ipi_srv_conf_t *prev = parent;
	ngx_http_51D_ipi_srv_conf_t *conf = child;

	ngx_conf_merge_uint_value(
		conf->matchConf.headerCount, prev->matchConf.headerCount, 0);

	return NGX_CONF_OK;
}

/**
 * Shared memory alloc function. Replaces fiftyoneDegreesMalloc to store
 * the data set in the shared memory zone.
 * @param __size the size of memory to allocate.
 * @return void* a pointer to the allocated memory.
 */
static void *ngx_http_51D_ipi_shm_alloc(size_t __size)
{
	void *ptr = NULL;
	ngx_slab_pool_t *shpool;
	shpool =
		(ngx_slab_pool_t *)ngx_http_51D_ipi_shm_resource_manager->shm.addr;
	ptr = ngx_slab_alloc_locked(shpool, __size);
	ngx_log_debug2(
		NGX_LOG_DEBUG_ALL,
		ngx_cycle->log,
		0,
		"51Degrees ipi shm alloc %d %p",
		__size,
		ptr);
	if (ptr == NULL) {
		ngx_log_error(
			NGX_LOG_ERR,
			ngx_cycle->log,
			0,
			"51Degrees ipi shm failed to allocate memory, not enough "
			"shared memory.");
	}
	return ptr;
}

/**
 * Shared memory alloc aligned function. If the __size is not multiple of
 * alignment, it will be rounded up. See the equivalent function in the
 * device detection module for the reasoning behind using the standard
 * shared memory alloc for aligned allocations.
 * @param alignment of the requested memory block
 * @param __size to be allocated
 * @return pointer to the allocated memory
 */
static void *ngx_http_51D_ipi_shm_alloc_aligned(int alignment, size_t __size)
{
	size_t actualAllocSize =
		__size % alignment ? (__size / alignment + 1) * alignment : __size;
	return ngx_http_51D_ipi_shm_alloc(actualAllocSize);
}

/**
 * Shared memory free function. Replaces fiftyoneDegreesFree to free
 * pointers to the shared memory zone.
 * @param __ptr pointer to the memory to be freed.
 */
static void ngx_http_51D_ipi_shm_free(void *__ptr)
{
	ngx_slab_pool_t *shpool;
	shpool =
		(ngx_slab_pool_t *)ngx_http_51D_ipi_shm_resource_manager->shm.addr;
	if ((u_char *) __ptr < shpool->start || (u_char *) __ptr > shpool->end) {
		// The memory is not in the shared memory pool, so free with
		// standard free function.
		ngx_log_debug1(
			NGX_LOG_DEBUG_ALL,
			ngx_cycle->log,
			0,
			"51Degrees ipi shm free (non shared) %p",
			__ptr);
		free(__ptr);
	}
	else {
		ngx_log_debug1(
			NGX_LOG_DEBUG_ALL,
			ngx_cycle->log,
			0,
			"51Degrees ipi shm free %p",
			__ptr);
		ngx_slab_free_locked(shpool, __ptr);
	}
}

/**
 * Init resource manager memory zone. Allocates space for the resource
 * manager in the shared memory zone.
 * @param shm_zone the shared memory zone.
 * @param data if the zone has been carried over from a reload, this is the
 *        old data.
 * @return ngx_int_t nginx conf status.
 */
static ngx_int_t
ngx_http_51D_ipi_init_shm_resource_manager(
	ngx_shm_zone_t *shm_zone, void *data)
{
	ngx_slab_pool_t *shpool;
	ResourceManager *resourceManager;
	shpool =
		(ngx_slab_pool_t *)ngx_http_51D_ipi_shm_resource_manager->shm.addr;

	// Allocate space for the resource manager.
	resourceManager =
		(ResourceManager *)ngx_slab_alloc(shpool, sizeof(ResourceManager));

	// Set the resource manager as the shared data for this zone.
	shm_zone->data = resourceManager;
	if (resourceManager == NULL) {
		ngx_log_error(
			NGX_LOG_ERR,
			shm_zone->shm.log,
			0,
			"51Degrees ipi shared memory could not be allocated.");
		return NGX_ERROR;
	}
	ngx_log_debug1(
		NGX_LOG_DEBUG_ALL,
		shm_zone->shm.log,
		0,
		"51Degrees ipi initialised shared memory with size %d.",
		shm_zone->shm.size);

	return NGX_OK;
}

/**
 * Init module function. Initialises the resource manager with the given
 * initialisation parameters. Throws an error if the resource manager could
 * not be initialised.
 * @param cycle the current nginx cycle.
 * @return ngx_int_t nginx conf status.
 */
static ngx_int_t
ngx_http_51D_ipi_init_module(ngx_cycle_t *cycle)
{
	ngx_http_51D_ipi_main_conf_t *fdmcf;
	ngx_slab_pool_t *shpool;

	// Get module main config.
	fdmcf = ngx_http_cycle_get_module_main_conf(
		cycle, ngx_http_51D_ipi_module);

	if (ngx_http_51D_ipi_shm_resource_manager == NULL) {
		return NGX_OK;
	}
	fdmcf->resourceManager =
		(ResourceManager *)ngx_http_51D_ipi_shm_resource_manager->data;

	shpool =
		(ngx_slab_pool_t *)ngx_http_51D_ipi_shm_resource_manager->shm.addr;

	// Set the memory allocation function to use the shared memory zone.
	Malloc = ngx_http_51D_ipi_shm_alloc;
	Free = ngx_http_51D_ipi_shm_free;
	MallocAligned = ngx_http_51D_ipi_shm_alloc_aligned;
	FreeAligned = ngx_http_51D_ipi_shm_free;

	// Initialise the resource manager.
	ngx_shmtx_lock(&shpool->mutex);
	ngx_http_51D_ipi_worker_count =
		(ngx_atomic_t *)ngx_http_51D_ipi_shm_alloc(sizeof(ngx_atomic_t));
	ngx_atomic_cmp_set(ngx_http_51D_ipi_worker_count, 0, 0);

	// Need to determine the ConfigIpi at this point
	ConfigIpi config = get_config_ipi(fdmcf);
	PropertiesRequired properties = get_properties_ipi(fdmcf);

	EXCEPTION_CREATE;
	IpiInitManagerFromFile(
		fdmcf->resourceManager,
		&config,
		&properties,
		(const char *)fdmcf->dataFile.data,
		exception
	);
	// Release lock
	ngx_shmtx_unlock(&shpool->mutex);

	// Reset the malloc and free functions as nothing else should be
	// allocated in the shared memory zone.
	Malloc = MemoryStandardMalloc;
	Free = MemoryStandardFree;
	MallocAligned = MemoryStandardMallocAligned;
	FreeAligned = MemoryStandardFreeAligned;

	if (EXCEPTION_FAILED) {
		return report_status(
			cycle->log,
			exception->status,
			(const char *)fdmcf->dataFile.data);
	}

	return NGX_OK;
}

/**
 * Init process function. Creates a results instance from the shared
 * resource manager. This results instance is local to the process.
 * @param cycle the current nginx cycle.
 * @return ngx_int_t nginx status.
 */
static ngx_int_t
ngx_http_51D_ipi_init_process(ngx_cycle_t *cycle)
{
	ngx_http_51D_ipi_main_conf_t *fdmcf;

	if (ngx_http_51D_ipi_shm_resource_manager == NULL) {
		return NGX_OK;
	}
	fdmcf = ngx_http_cycle_get_module_main_conf(
		cycle, ngx_http_51D_ipi_module);

	fdmcf->results = ResultsIpiCreate(fdmcf->resourceManager);

	if (fdmcf->results == NULL) {
		return report_insufficient_memory_status(cycle->log);
	}

	// Increment the workers which are using the dataset.
	ngx_atomic_fetch_add(ngx_http_51D_ipi_worker_count, 1);
	return NGX_OK;
}

/**
 * Exit process function. Frees the results instance that was created on
 * process init.
 * @param cycle the current nginx cycle.
 */
static void
ngx_http_51D_ipi_exit_process(ngx_cycle_t *cycle)
{
	ngx_http_51D_ipi_main_conf_t *fdmcf;

	if (ngx_http_51D_ipi_shm_resource_manager == NULL) {
		return;
	}

	fdmcf = ngx_http_cycle_get_module_main_conf(
		cycle, ngx_http_51D_ipi_module);

	ResultsIpiFree(fdmcf->results);

	// Decrement the worker count. Try 5 times if not succeed.
	ngx_uint_t i;
	for (
		i = 5;
		i > 0 &&
			ngx_atomic_fetch_add(ngx_http_51D_ipi_worker_count, -1) != 1;
		i--) {}
}

/**
 * Exit master process. Frees resources created for the module.
 * @param cycle the current nginx cycle.
 */
static void
ngx_http_51D_ipi_exit_master(ngx_cycle_t *cycle)
{
	ngx_slab_pool_t *shpool;
	ResourceManager *resourceManager;
	if (ngx_http_51D_ipi_shm_resource_manager == NULL) {
		return;
	}

	resourceManager =
		(ResourceManager *)ngx_http_51D_ipi_shm_resource_manager->data;
	shpool =
		(ngx_slab_pool_t *)ngx_http_51D_ipi_shm_resource_manager->shm.addr;

	// Lock the shared memory and free any memory allocated for the module
	ngx_shmtx_lock(&shpool->mutex);
	if (*ngx_http_51D_ipi_worker_count > 0) {
		ngx_log_error(
			NGX_LOG_WARN,
			cycle->log,
			0,
			"51Degrees ipi not all child processes has terminated at "
			"master process termination");
	}

	// All child process should have already been terminated at this point.
	// Also once the master has terminated, any running worker process
	// should not be of any use so proceed to free the resource.
	Free = ngx_http_51D_ipi_shm_free;
	FreeAligned = ngx_http_51D_ipi_shm_free;
	ResourceManagerFree(resourceManager);
	Free = MemoryStandardFree;
	FreeAligned = MemoryStandardFreeAligned;
	ngx_http_51D_ipi_shm_free((void *)resourceManager);
	ngx_http_51D_ipi_shm_free((void *)ngx_http_51D_ipi_worker_count);
	ngx_shmtx_unlock(&shpool->mutex);
}

/**
 * Definitions of the directives which can be called from the config file.
 * --51D_match_ipi takes two string arguments, the name of the header to be
 * set and a comma separated list of properties to be returned. The third
 * argument is optional to specify a variable holding an IP address to be
 * used in place of the client IP address. Is called within main, server
 * and location blocks. Enables IP intelligence matching.
 * --51D_file_path_ipi takes one string argument, the path to a 51Degrees
 * IP intelligence data file. Is called within the main block.
 * --51D_value_separator_ipi takes one string argument, the separator of
 * the values being returned.
 */
static ngx_command_t ngx_http_51D_ipi_commands[] = {

	{ ngx_string("51D_match_ipi"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_ipi_set_loc,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	{ ngx_string("51D_match_ipi"),
	NGX_HTTP_SRV_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_ipi_set_srv,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	{ ngx_string("51D_match_ipi"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_ipi_set_main,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	{ ngx_string("51D_file_path_ipi"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_str_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_ipi_main_conf_t, dataFile),
	NULL },

	{ ngx_string("51D_value_separator_ipi"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_str_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_ipi_main_conf_t, valueSeparator),
	NULL },

	ngx_null_command
};

/**
 * Module context. Sets the configuration functions.
 */
static ngx_http_module_t ngx_http_51D_ipi_module_ctx = {
	NULL,                              /* preconfiguration */
	ngx_http_51D_ipi_post_conf,        /* postconfiguration */

	ngx_http_51D_ipi_create_main_conf, /* create main configuration */
	NULL,                              /* init main configuration */

	ngx_http_51D_ipi_create_srv_conf,  /* create server configuration */
	ngx_http_51D_ipi_merge_srv_conf,   /* merge server configuration */

	ngx_http_51D_ipi_create_loc_conf,  /* create location configuration */
	ngx_http_51D_ipi_merge_loc_conf,   /* merge location configuration */
};

/**
 * Module definition. Set the module context, commands, type and init
 * function.
 */
ngx_module_t ngx_http_51D_ipi_module = {
	NGX_MODULE_V1,
	&ngx_http_51D_ipi_module_ctx,      /* module context */
	ngx_http_51D_ipi_commands,         /* module directives */
	NGX_HTTP_MODULE,                   /* module type */
	NULL,                              /* init master */
	ngx_http_51D_ipi_init_module,      /* init module */
	ngx_http_51D_ipi_init_process,     /* init process */
	NULL,                              /* init thread */
	NULL,                              /* exit thread */
	ngx_http_51D_ipi_exit_process,     /* exit process */
	ngx_http_51D_ipi_exit_master,      /* exit master */
	NGX_MODULE_V1_PADDING
};

/**
 * Add value function. Appends a string to a list separated by the
 * delimiter specified with 51D_value_separator_ipi, or a comma by default.
 * @param delimiter to split values with.
 * @param val the string to add to dst.
 * @param dst the string to append the val to.
 * @param length the size remaining to append val to dst.
 */
static void add_value(char *delimiter, char *val, char *dst, size_t length)
{
	// If the buffer already contains characters, append the delimiter.
	if (dst[0] != '\0') {
		strncat(dst, delimiter, length);
		length -= strlen(delimiter);
	}

	// Append the value.
	strncat(dst, val, length);
}

/**
 * Create an empty ngx string from the http request pool.
 * @param r pointer to a http request
 * @return pointer to an empty ngx string
 */
static ngx_str_t* empty_string(ngx_http_request_t *r) {
	ngx_str_t *str = (ngx_str_t*)ngx_palloc(r->pool, sizeof(ngx_str_t));
	if (str == NULL) {
		report_insufficient_memory_status(r->connection->log);
		return NULL;
	}

	str->len = 0;
	str->data = (u_char *)ngx_palloc(r->pool, 1);
	if (str->data == NULL) {
		report_insufficient_memory_status(r->connection->log);
		return NULL;
	}

	str->data[0] = '\0';
	return str;
}

/**
 * Take a null terminated copy of an nginx string. The IP address parser
 * in the engine reads one byte beyond the characters provided, so a string
 * which is not null terminated, such as the connection address text, must
 * be copied before being used for a match.
 * @param r the nginx http request
 * @param str the string to copy
 * @return the null terminated copy, or NULL if memory could not be
 * allocated
 */
static ngx_str_t *
copy_string_null_terminated(ngx_http_request_t *r, ngx_str_t *str) {
	ngx_str_t *copy = (ngx_str_t *)ngx_palloc(r->pool, sizeof(ngx_str_t));
	if (copy == NULL) {
		report_insufficient_memory_status(r->connection->log);
		return NULL;
	}
	copy->data = (u_char *)ngx_palloc(r->pool, str->len + 1);
	if (copy->data == NULL) {
		report_insufficient_memory_status(r->connection->log);
		return NULL;
	}
	ngx_memcpy(copy->data, str->data, str->len);
	copy->len = str->len;
	copy->data[copy->len] = '\0';
	return copy;
}

/**
 * Search and get the value of an nginx variable, unescaping any URI
 * encoding. Used to obtain the IP address when a variable is specified
 * with a 51D_match_ipi directive.
 * @param r the nginx http request
 * @param variableName the variable name to search for
 * @return the value of the variable, or an empty string if not found
 */
static ngx_str_t *
get_evidence_from_variable(ngx_http_request_t *r, ngx_str_t *variableName) {
	u_char *src, *dst;
	ngx_uint_t variableNameHash;
	ngx_variable_value_t *variable;
	ngx_str_t *evidence;

	variableNameHash = ngx_hash_strlow(
		variableName->data,
		variableName->data,
		variableName->len);
	variable = ngx_http_get_variable(r, variableName, variableNameHash);
	if (variable != NULL &&
		variable->not_found == 0 &&
		(int)variable->len > 0) {
		evidence = (ngx_str_t *)ngx_palloc(r->pool, sizeof(ngx_str_t));
		if (evidence == NULL) {
			report_insufficient_memory_status(r->connection->log);
			return NULL;
		}

		evidence->data =
			(u_char *)ngx_palloc(
				r->pool, sizeof(u_char) * (size_t)variable->len + 1);
		if (evidence->data == NULL) {
			report_insufficient_memory_status(r->connection->log);
			return NULL;
		}

		src = variable->data;
		dst = evidence->data;
		ngx_unescape_uri(&dst, &src, variable->len, 0);
		evidence->len = dst - evidence->data;
		evidence->data[evidence->len] = '\0';
	}
	else {
		evidence = empty_string(r);
	}
	return evidence;
}

/**
 * Get match function. Performs an IP intelligence match for the IP
 * address provided.
 * @param fdmcf module main config.
 * @param r the current HTTP request.
 * @param ipAddress the IP address to perform the match on.
 * @return Nginx status code.
 */
static ngx_uint_t ngx_http_51D_ipi_get_match(
	ngx_http_51D_ipi_main_conf_t *fdmcf,
	ngx_http_request_t *r,
	ngx_str_t *ipAddress)
{
	EXCEPTION_CREATE
	ResultsIpiFromIpAddressString(
		fdmcf->results,
		(const char *)ipAddress->data,
		ipAddress->len,
		exception);
	if (EXCEPTION_FAILED) {
		return report_status(
			r->connection->log,
			exception->status,
			(const char *)fdmcf->dataFile.data);
	}
	return NGX_OK;
}

/**
 * Get value function. Gets the requested value for the current match and
 * appends the value to the list of values separated by the delimiter
 * specified with 51D_value_separator_ipi.
 * @param fdmcf module main config.
 * @param r the current HTTP request.
 * @param values_string the string to append the returned value to.
 * @param requiredPropertyName the name of the property to get the value
 * for.
 * @param length the size allocated to the values_string variable.
 */
static void ngx_http_51D_ipi_get_value(
	ngx_http_51D_ipi_main_conf_t *fdmcf,
	ngx_http_request_t *r,
	char *values_string,
	const char *requiredPropertyName,
	size_t length)
{
	size_t remainingLength = length - strlen(values_string);
	ResultsIpi *results = fdmcf->results;
	DataSetIpi *dataSet = (DataSetIpi *)results->b.dataSet;
	const char *valueDelimiter = (const char *)fdmcf->valueSeparator.data;
	ngx_int_t charsAdded = 0;
	char *dest = values_string + strlen(values_string);

	if (values_string[0] != '\0') {
		charsAdded = snprintf(dest, remainingLength, "%s", valueDelimiter);
		if (charsAdded >= 0 && charsAdded < (ngx_int_t)remainingLength) {
			remainingLength -= strlen(valueDelimiter);
			dest += strlen(valueDelimiter);
		}
	}

	if (charsAdded >= 0 && charsAdded < (ngx_int_t)remainingLength) {
		EXCEPTION_CREATE
		int requiredPropertyIndex =
			PropertiesGetRequiredPropertyIndexFromName(
				dataSet->b.b.available, requiredPropertyName);

		bool hasValues = ResultsIpiGetHasValues(
			results,
			requiredPropertyIndex,
			exception);
		if (hasValues) {
			charsAdded = ResultsIpiGetValuesString(
				results,
				requiredPropertyName,
				dest,
				remainingLength,
				"|",
				exception);
			if (EXCEPTION_FAILED) {
				report_status(
					r->connection->log,
					exception->status,
					(const char *)fdmcf->dataFile.data);
			}
		}
		else {
			charsAdded = snprintf(
				dest, remainingLength,
				"%s",
				FIFTYONE_DEGREES_IPI_PROPERTY_NOT_AVAILABLE);
		}
	}

	if (charsAdded < 0) {
		ngx_log_error(
			NGX_LOG_ERR,
			r->connection->log,
			0,
			"51Degrees ipi failed to construct value string.");
	}
	else if (charsAdded > (ngx_int_t)remainingLength) {
		ngx_log_error(
			NGX_LOG_WARN,
			r->connection->log,
			0,
			"51Degrees ipi value string is bigger than the available "
			"buffer.");
	}
}

/**
 * Perform a match if required and return an escaped value string for the
 * header to set.
 * @param r a ngx_http_request_t.
 * @param fdmcf a main config object.
 * @param header a header to construct a value string for.
 * @param haveMatch whether a match has already been performed for the
 * input IP address.
 * @param ipAddress an IP address string to perform the match on.
 * @return an escaped value string. NULL if an error occurred.
 */
static u_char *getEscapedMatchedValueString(
	ngx_http_request_t *r,
	ngx_http_51D_ipi_main_conf_t *fdmcf,
	ngx_http_51D_ipi_data_to_set *header,
	int haveMatch,
	ngx_str_t *ipAddress)
{
	memset(fdmcf->valueString, 0, FIFTYONE_DEGREES_IPI_MAX_STRING);

	// Get a match. If there are multiple instances of 51D_match_ipi, then
	// don't get the match if it has already been fetched.
	if (haveMatch == 0) {
		ngx_uint_t ngxCode =
			ngx_http_51D_ipi_get_match(fdmcf, r, ipAddress);
		if (ngxCode != NGX_OK) {
			return NULL;
		}
	}

	// For each property, set the value in the value string.
	int property_index;
	for (property_index = 0;
		property_index < (int)header->propertyCount;
		property_index++) {
		ngx_http_51D_ipi_get_value(
			fdmcf,
			r,
			fdmcf->valueString,
			(const char *)header->property[property_index]->data,
			FIFTYONE_DEGREES_IPI_MAX_STRING);
	}

	// Escape characters which cannot be added in a header value, most
	// importantly '\n' and '\r'.
	size_t valueStringLength = strlen(fdmcf->valueString);
	size_t escapedChars =
		(size_t)ngx_escape_json(
			NULL, (u_char *)fdmcf->valueString, valueStringLength);
	u_char *escapedValueString =
		(u_char *)ngx_palloc(
			r->pool, (valueStringLength + escapedChars + 1) * sizeof(char));
	if (escapedValueString == NULL) {
		report_insufficient_memory_status(r->connection->log);
		return NULL;
	}
	ngx_escape_json(
		escapedValueString, (u_char *)fdmcf->valueString, valueStringLength);
	escapedValueString[valueStringLength + escapedChars] = '\0';

	return escapedValueString;
}

/**
 * Process a request by performing a match and setting the resulting
 * property values as a request header.
 * @param r the http request
 * @param fdmcf the main configuration of the module
 * @param header the header to set
 * @param haveMatch indicates if a match has already been performed for the
 * input IP address
 * @param ipAddress the IP address to perform the match for
 * @return code to indicate the status of the operation.
 */
static ngx_uint_t
process(ngx_http_request_t *r,
		ngx_http_51D_ipi_main_conf_t *fdmcf,
		ngx_http_51D_ipi_data_to_set *header,
		int haveMatch,
		ngx_str_t *ipAddress)
{
	ngx_table_elt_t *h;
	u_char *escapedValueString = getEscapedMatchedValueString(
		r, fdmcf, header, haveMatch, ipAddress);
	if (escapedValueString == NULL) {
		return NGX_ERROR;
	}

	// Set a new header name and value.
	h = ngx_list_push(&r->headers_in.headers);
	if (h == NULL) {
		report_insufficient_memory_status(r->connection->log);
		return NGX_ERROR;
	}
	h->key.data = (u_char *)header->headerName.data;
	h->key.len = ngx_strlen(h->key.data);
	h->hash = ngx_hash_key(h->key.data, h->key.len);
	h->value.data = escapedValueString;
	h->value.len = ngx_strlen(h->value.data);
	h->lowcase_key = (u_char *)header->lowerHeaderName.data;
	return NGX_OK;
}

/**
 * Module handler, gets a match using the client IP address, or an IP
 * address held in the variable specified by the directive, then sets the
 * requested properties as request headers.
 * @param r the HTTP request.
 * @return ngx_int_t nginx status.
 */
static ngx_int_t
ngx_http_51D_ipi_handler(ngx_http_request_t *r)
{
	ngx_http_51D_ipi_main_conf_t *fdmcf;
	ngx_http_51D_ipi_srv_conf_t *fdscf;
	ngx_http_51D_ipi_loc_conf_t *fdlcf;
	ngx_http_51D_ipi_match_conf_t
		*matchConf[FIFTYONE_DEGREES_IPI_CONFIG_LEVELS];
	ngx_uint_t haveMatch;
	ngx_http_51D_ipi_data_to_set *currentHeader;
	int totalHeaderCount, matchConfIndex;
	ngx_str_t *ipAddress, *nextIpAddress;

	// Use a module context as a marker to ensure that the matching is
	// only performed once per request. The request internal flag cannot
	// be used here as it may be set by other modules, including the
	// 51Degrees device detection module.
	if (ngx_http_51D_ipi_shm_resource_manager == NULL ||
		r->headers_in.headers.last == NULL ||
		ngx_http_get_module_ctx(r, ngx_http_51D_ipi_module) != NULL) {
		return NGX_DECLINED;
	}
	ngx_http_set_ctx(r, &ngx_http_51D_ipi_module, ngx_http_51D_ipi_module);

	// Get the module configs.
	fdlcf = ngx_http_get_module_loc_conf(r, ngx_http_51D_ipi_module);
	fdscf = ngx_http_get_module_srv_conf(r, ngx_http_51D_ipi_module);
	fdmcf = ngx_http_get_module_main_conf(r, ngx_http_51D_ipi_module);

	matchConf[0] = &fdlcf->matchConf;
	matchConf[1] = &fdscf->matchConf;
	matchConf[2] = &fdmcf->matchConf;

	totalHeaderCount = matchConf[0]->headerCount +
					   matchConf[1]->headerCount +
					   matchConf[2]->headerCount;

	// If no headers are specified in this location, tell nginx
	// this handler is done.
	if ((int)totalHeaderCount == 0) {
		return NGX_DECLINED;
	}

	// Perform the matches which use the client IP address. The match is
	// performed once and reused for all the headers to set. The address
	// text held by nginx is not null terminated so a null terminated copy
	// is used for the match.
	ipAddress = copy_string_null_terminated(r, &r->connection->addr_text);
	haveMatch = 0;
	for (matchConfIndex = 0;
		ipAddress != NULL &&
		matchConfIndex < FIFTYONE_DEGREES_IPI_CONFIG_LEVELS;
		matchConfIndex++) {
		currentHeader = matchConf[matchConfIndex]->header;
		while (currentHeader != NULL) {
			if ((int)currentHeader->variableName.len <= 0) {
				haveMatch =
					(process(r, fdmcf, currentHeader, haveMatch, ipAddress)
						== NGX_OK);
			}
			currentHeader = currentHeader->next;
		}
	}

	// Perform the matches which use an IP address held in a variable.
	// Where consecutive matches use the same IP address the previous
	// match is reused.
	ipAddress = &(ngx_str_t)ngx_string("");
	for (matchConfIndex = 0;
		matchConfIndex < FIFTYONE_DEGREES_IPI_CONFIG_LEVELS;
		matchConfIndex++) {
		currentHeader = matchConf[matchConfIndex]->header;
		while (currentHeader != NULL) {
			if ((int)currentHeader->variableName.len > 0) {
				nextIpAddress = get_evidence_from_variable(
					r, &currentHeader->variableName);
				if (nextIpAddress != NULL) {
					if (ipAddress != NULL &&
						strcmp(
							(const char *)ipAddress->data,
							(const char *)nextIpAddress->data) == 0) {
						process(r, fdmcf, currentHeader, 1, ipAddress);
					}
					else {
						ipAddress = nextIpAddress;
						process(r, fdmcf, currentHeader, 0, ipAddress);
					}
				}
			}
			currentHeader = currentHeader->next;
		}
	}

	// Tell nginx to continue with other module handlers.
	return NGX_DECLINED;
}

/**
 * Set data function. Initialises the data structure for a given occurrence
 * of "51D_match_ipi" in the config file. Allocates space required and sets
 * the name and properties.
 * @param cf the nginx config.
 * @param data data to be set.
 * @param value the values passed from the config file.
 * @param fdmcf the module main config.
 * @return Nginx status code.
 */
static char *
set_data(
	ngx_conf_t *cf,
	ngx_http_51D_ipi_data_to_set *data,
	ngx_str_t *value,
	ngx_http_51D_ipi_main_conf_t *fdmcf)
{
	char *tok, *tokPos = NULL;
	int propertiesCount, charPos;
	char *propertiesString;

	// Initialise the property count.
	data->propertyCount = 0;

	// Set the name of the header.
	data->headerName.data = (u_char *)ngx_palloc(cf->pool, value[1].len + 1);
	data->lowerHeaderName.data =
		(u_char *)ngx_palloc(cf->pool, value[1].len + 1);
	if (data->headerName.data == NULL ||
		data->lowerHeaderName.data == NULL) {
		report_insufficient_memory_status(cf->log);
		return NGX_CONF_ERROR;
	}
	strcpy((char *)data->headerName.data, (char *)value[1].data);
	data->headerName.len = value[1].len;
	ngx_strlow(
		data->lowerHeaderName.data,
		data->headerName.data,
		data->headerName.len);
	data->lowerHeaderName.len = data->headerName.len;

	// Set the properties string to the second argument in the config file.
	propertiesString = (char *)value[2].data;

	// Count the properties in the string.
	propertiesCount = 1;
	for (charPos = 0; charPos < (int)value[2].len; charPos++) {
		if (propertiesString[charPos] == ',') {
			propertiesCount++;
		}
	}

	// Allocate space for the properties array.
	data->property =
		(ngx_str_t **)ngx_palloc(
			cf->pool, sizeof(ngx_str_t *) * propertiesCount);
	if (data->property == NULL) {
		report_insufficient_memory_status(cf->log);
		return NGX_CONF_ERROR;
	}

	tok = strtok(propertiesString, ",");
	while (tok != NULL) {
		data->property[data->propertyCount] =
			(ngx_str_t *)ngx_palloc(cf->pool, sizeof(ngx_str_t));
		if (data->property[data->propertyCount] == NULL) {
			report_insufficient_memory_status(cf->log);
			return NGX_CONF_ERROR;
		}

		data->property[data->propertyCount]->data =
			(u_char *)ngx_palloc(
				cf->pool, sizeof(u_char) * (ngx_strlen(tok) + 1));
		if (data->property[data->propertyCount]->data == NULL) {
			report_insufficient_memory_status(cf->log);
			return NGX_CONF_ERROR;
		}

		strcpy(
			(char *)data->property[data->propertyCount]->data,
			(const char *)tok);
		data->property[data->propertyCount]->len =
			ngx_strlen(data->property[data->propertyCount]->data);

		// A property is not already included if it does not present in the
		// composed properties string and is not a substring of other
		// already presented properties.
		tokPos = ngx_strstr(fdmcf->properties, tok);
		if (tokPos == NULL ||
			(tokPos != NULL &&
				(tokPos + ngx_strlen(tok))[0] != ',' &&
				(tokPos + ngx_strlen(tok))[0] != '\0')) {
			add_value(
				",",
				tok,
				fdmcf->properties,
				FIFTYONE_DEGREES_IPI_MAX_PROPS_STRING -
					strlen(fdmcf->properties));
		}
		data->propertyCount++;
		tok = strtok(NULL, ",");
	}

	// Set the variable name if it is specified.
	if ((int)cf->args->nelts >= 4) {
		if (value[3].data[0] == '$') {
			data->variableName.data = value[3].data + 1;
			data->variableName.len = value[3].len - 1;
		}
		else {
			ngx_log_error(
				NGX_LOG_ERR,
				cf->cycle->log,
				0,
				"51Degrees argument '%s' is not in a valid format",
				(const char *)value[3].data);
			return NGX_CONF_ERROR;
		}
	}
	else {
		data->variableName = (ngx_str_t)ngx_null_string;
	}
	return NGX_CONF_OK;
}

/**
 * Set function. Is called for each occurrence of "51D_match_ipi".
 * Allocates space for the header structure and initialises it with the set
 * data function.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param matchConf the match config.
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_ipi_set_conf_header(
	ngx_conf_t *cf,
	ngx_command_t *cmd,
	ngx_http_51D_ipi_match_conf_t *matchConf)
{
	ngx_http_51D_ipi_main_conf_t *fdmcf;
	ngx_http_51D_ipi_data_to_set *header;
	ngx_str_t *value;
	char *status;

	// Get the arguments from the config.
	value = cf->args->elts;

	// Get the module main config.
	fdmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_51D_ipi_module);

	// Get the end of the header list.
	if (matchConf->header == NULL)
	{
		// Allocate the space for the first header in the current config.
		matchConf->header =
			(ngx_http_51D_ipi_data_to_set*)ngx_palloc(
				cf->pool, sizeof(ngx_http_51D_ipi_data_to_set));
		if (matchConf->header == NULL) {
			report_insufficient_memory_status(cf->log);
			return NGX_CONF_ERROR;
		}

		header = matchConf->header;
	}
	else {
		header = matchConf->header;
		while (header->next != NULL)
		{
			header = header->next;
		}
		// Allocate the space for the next header in the current config.
		header->next =
			(ngx_http_51D_ipi_data_to_set*)ngx_palloc(
				cf->pool, sizeof(ngx_http_51D_ipi_data_to_set));
		if (header->next == NULL) {
			report_insufficient_memory_status(cf->log);
			return NGX_CONF_ERROR;
		}

		header = header->next;
	}

	// Set the next pointer to NULL to show it is the last in the list.
	header->next = NULL;

	// Set the properties for the selected location.
	status = set_data(cf, header, value, fdmcf);
	if (status != NGX_CONF_OK) {
		return status;
	}

	matchConf->headerCount++;

	return NGX_CONF_OK;
}

/**
 * Set function. Is called for occurrences of "51D_match_ipi" in a location
 * config block.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_ipi_set_loc(
	ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_ipi_loc_conf_t *fdlcf =
		ngx_http_conf_get_module_loc_conf(cf, ngx_http_51D_ipi_module);
	return ngx_http_51D_ipi_set_conf_header(cf, cmd, &fdlcf->matchConf);
}

/**
 * Set function. Is called for occurrences of "51D_match_ipi" in a server
 * config block.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_ipi_set_srv(
	ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_ipi_srv_conf_t *fdscf =
		ngx_http_conf_get_module_srv_conf(cf, ngx_http_51D_ipi_module);
	return ngx_http_51D_ipi_set_conf_header(cf, cmd, &fdscf->matchConf);
}

/**
 * Set function. Is called for occurrences of "51D_match_ipi" in a http
 * config block.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_ipi_set_main(
	ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_ipi_main_conf_t *fdmcf =
		ngx_http_conf_get_module_main_conf(cf, ngx_http_51D_ipi_module);
	return ngx_http_51D_ipi_set_conf_header(cf, cmd, &fdmcf->matchConf);
}

/**
 * @}
 */
