/* *********************************************************************
 * This Original Work is copyright of 51 Degrees Mobile Experts Limited.
 * Copyright 2023 51 Degrees Mobile Experts Limited, Davidson House,
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

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_rbtree.h>
#include <ngx_string.h>
#include <inttypes.h>
#include "src/hash/hash.h"
#undef MAP_TYPE
#include "src/hash/fiftyone.h"

/**
 * @defgroup ngx_http_51D_module 51Degrees Nginx Module Internals
 *
 * @{
 */
 
/**
 * Default data file to use.
 */
#define FIFTYONE_DEGREES_DEFAULTFILE (u_char*) "51Degrees-LiteV4.1.hash"

#ifndef FIFTYONE_DEGREES_PROPERTY_NOT_AVAILABLE
/**
 * Default property value if it is not available or no match is found.
 */
#define FIFTYONE_DEGREES_PROPERTY_NOT_AVAILABLE "NoMatch"
#endif

#ifndef FIFTYONE_DEGREES_JAVASCRIPT_NOT_AVAILABLE
/**
 * Default Javascript content if it is not available or no match is found.
 */
#define FIFTYONE_DEGREES_JAVASCRIPT_NOT_AVAILABLE "/* 51Degrees Javascript not available. */"
#endif

#ifndef FIFTYONE_DEGREES_VALUE_SEPARATOR
/**
 * Default separator.
 */
#define FIFTYONE_DEGREES_VALUE_SEPARATOR (u_char*) ","
#endif

#ifndef FIFTYONE_DEGREES_MAX_STRING
/** 
 * Maximum value string being returned.
 * The biggest Javascript is 11666 so choose a number
 * that is reasonably bigger than as as max length
 */
#define FIFTYONE_DEGREES_MAX_STRING 20000
#endif

#ifndef FIFTYONE_DEGREES_MAX_PROPS_STRING
/**
 * Maxium size of string hold full properties list
 */
#define FIFTYONE_DEGREES_MAX_PROPS_STRING 2048
#endif

#ifndef FIFTYONE_DEGREES_SET_HEADER_PREFIX
/**
 * The SetHeader prefix value.
 */
#define FIFTYONE_DEGREES_SET_HEADER_PREFIX "SetHeader"
#endif

/**
 * NGINX shared memory zone requires minimum of 8 bytes
 * and allocated size has to be power of 2. Any non-
 * conforming value will be rounded up. Thus make this
 * adjustment value account for the extra rounded up
 * memory.
 */
#define FIFTYONE_DEGREES_MEMORY_ADJUSTMENT 1.1

/**
 * True constant.
 */
#define FIFTYONE_DEGREES_TRUE 1

/**
 * False constant.
 */
#define FIFTYONE_DEGREES_FALSE 0

/**
 * 3 Config levels main, server and location.
 */
#define FIFTYONE_DEGREES_CONFIG_LEVELS 3

/**
 * Global module declaration.
 */
ngx_module_t ngx_http_51D_module;

// Configuration function declarations.
/**
 * Forward declaration of #ngx_http_51D_create_main_conf.
 */
static void *ngx_http_51D_create_main_conf(ngx_conf_t *cf);
/**
 * Forward declaration of #ngx_http_51D_create_srv_conf.
 */
static void *ngx_http_51D_create_srv_conf(ngx_conf_t *cf);
/**
 * Forward declaration of #ngx_http_51D_create_loc_conf.
 */
static void *ngx_http_51D_create_loc_conf(ngx_conf_t *cf);
/**
 * Forward declaration of #ngx_http_51D_merge_loc_conf.
 */
static char *ngx_http_51D_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
/**
 * Forward declaration of #ngx_http_51D_merge_srv_conf.
 */
static char *ngx_http_51D_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child);

// Set function declarations.
/**
 * Forward declaration of #ngx_http_51D_set_loc_resp.
 */
static char *ngx_http_51D_set_loc_resp(ngx_conf_t* cf, ngx_command_t *cmd, void *conf);
/**
 * Forward declaration of #ngx_http_51D_set_srv_resp.
 */
static char *ngx_http_51D_set_srv_resp(ngx_conf_t* cf, ngx_command_t *cmd, void *conf);
/**
 * Forward declaration of #ngx_http_51D_set_main_resp.
 */
static char *ngx_http_51D_set_main_resp(ngx_conf_t* cf, ngx_command_t *cmd, void *conf);
/**
 * Forward declaration of #ngx_http_51D_set_loc_cdn.
 */
static char *ngx_http_51D_set_loc_cdn(ngx_conf_t* cf, ngx_command_t *cmd, void *conf);
/**
 * Forward declaration of #ngx_http_51D_set_loc.
 */
static char *ngx_http_51D_set_loc(ngx_conf_t* cf, ngx_command_t *cmd, void *conf);
/**
 * Forward declaration of #ngx_http_51D_set_srv.
 */
static char *ngx_http_51D_set_srv(ngx_conf_t* cf, ngx_command_t *cmd, void *conf);
/**
 * Forward declaration of #ngx_http_51D_set_main.
 */
static char *ngx_http_51D_set_main(ngx_conf_t* cf, ngx_command_t *cmd, void *conf);

// Request handler declaration.
/**
 * Forward declaration of #ngx_http_51D_handler.
 */
static ngx_int_t ngx_http_51D_handler(ngx_http_request_t *r);
/**
 * Forward declaration of #ngx_http_51D_header_filter.
 */
static ngx_int_t ngx_http_51D_header_filter(ngx_http_request_t *r);
/**
 * Forward declaration of #ngx_http_51D_body_filter.
 */
static ngx_int_t ngx_http_51D_body_filter(ngx_http_request_t *r, ngx_chain_t *in);

/**
 * Declaration of the next header filter function pointer.
 */
static ngx_http_output_header_filter_pt ngx_http_next_header_filter;
/**
 * Declaration of the next body filter function pointer.
 */
static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

/**
 * Pointer to shared memory zones.
 */
static ngx_shm_zone_t *ngx_http_51D_shm_resource_manager;
/**
 * Forward declaration of #ngx_http_51D_init_shm_resource_manager
 */
static ngx_int_t ngx_http_51D_init_shm_resource_manager(ngx_shm_zone_t *shm_zone, void *data);
/**
 * Atomic integer used to ensure a new data set memory zone on each reload.
 */
ngx_atomic_t ngx_http_51D_shm_tag = 1;
/**
 * Count number of current worker processes.
 */
ngx_atomic_t *ngx_http_51D_worker_count;

/**
 * Performance profiles.
 */
typedef enum ngx_http_51D_performance_profile_e ngx_http_51D_performance_profile;

/**
 * Performance profiles enumerator.
 * NOTE: Only in_memory is currently supported.
 */
enum ngx_http_51D_performance_profile_e {
	ngx_http_51D_in_memory = 1,
	ngx_http_51D_high_performance,
	ngx_http_51D_low_memory,
	ngx_http_51D_balanced,
	ngx_http_51D_balanced_temp
};

/**
 * Multi-header detection mode
 */
typedef enum ngx_http_51D_multi_header_mode_e ngx_http_51D_multi_header_mode;

/**
 * Multi-header detection mode enumerator.
 */
enum ngx_http_51D_multi_header_mode_e {
	ngx_http_51D_ua_only = 0,
	ngx_http_51D_all,
	ngx_http_51D_client_hints,
	ngx_http_51D_multi_header_mode_count,
};

/**
 * Structure containing details of a specific header to be set as per the
 * config file.
 */
typedef struct ngx_http_51D_data_to_set_t ngx_http_51D_data_to_set;

struct ngx_http_51D_data_to_set_t {
    ngx_uint_t multi;                       /**< see `ngx_http_51D_multi_header_mode` */
    ngx_uint_t propertyCount;               /**< The number of properties in the
                                                 property array. */
    ngx_str_t **property;                   /**< Array of properties to set. */
    ngx_str_t headerName;                   /**< The header name to set. */
    ngx_str_t lowerHeaderName;              /**< The header name in lower case. */
    ngx_str_t variableName;                 /**< The name of the variable to use
                                                 a User-Agent */
    ngx_http_51D_data_to_set *next;         /**< The next header in the list. */
};

/**
 * Match config structure set from the config file.
 */
typedef struct {
	ngx_uint_t multiMask;                /**< Indicated whether there is a
	                                       	  multiple HTTP header match in this
	                                          location.
											  Is a bit mask using shifts from
											  `ngx_http_51D_multi_header_mode_e`. */
    ngx_uint_t setHeaders;				 /**< Indicates if response headers
											  should be set. */
	ngx_uint_t headerCount;              /**< The number of headers to set. */
    ngx_http_51D_data_to_set *header;    /**< List of headers to set. */
	ngx_http_51D_data_to_set *body;      /**< Body to set. There can be only 
											  one per config. */
} ngx_http_51D_match_conf_t;

/**
 * Module location config.
 */
typedef struct {
	ngx_http_51D_match_conf_t matchConf; /**< The match to carry out in this
	                                          location. */
} ngx_http_51D_loc_conf_t;

/**
 * Module main config.
 */
typedef struct {
	char properties[FIFTYONE_DEGREES_MAX_PROPS_STRING]; /**< Properties string to
	                                                   initialise the data
	                                                   set with. */
    ngx_str_t dataFile;                           /**< 51Degrees data file. */
	ResultsHash *results;                         /**< 51Degrees results, local
	                                                   to each process. */
	char valueString[FIFTYONE_DEGREES_MAX_STRING]; /**< Result string to hold
												       return javascript */
	ResourceManager *resourceManager;             /**< 51Degrees data set,
                                                       shared across all 
                                                       process'. */
	ngx_uint_t setRespHeaderCount;			  	  /**< Number of response headers
													   to set, local to each to
													   each process. */
	ngx_http_51D_data_to_set *setRespHeader;      /**< Array of headers to set
													   in the response, local
													   to each process. */
	ngx_str_t valueSeparator;                     /**< Match header value
                                                       separator. */
	ngx_uint_t performanceProfile;                /**< 51Degrees
                                                       performance profiles. */
	ngx_uint_t drift;                             /**< 51Degrees drift value to 
                                                       set. */
	ngx_uint_t difference;                        /**< 51Degrees difference value
                                                       to set. */
	ngx_uint_t maxConcurrency;                    /**< 51Degrees max concurrency 
                                                       value to set. */
	ngx_uint_t allowUnmatched;                    /**< 51Degrees flag, whether
                                                       unmatched should be
                                                       allowed. */ 
	ngx_uint_t usePerformanceGraph;               /**< 51Degrees flag, whether
                                                       performance graph should
                                                       be used. */
	ngx_uint_t usePredictiveGraph;                /**< 51Degrees flag, whether
                                                       predictive graph should
                                                       be used. */
	ngx_http_51D_match_conf_t matchConf;          /**< The match to carry out in
	                                                   this block's locations. */
} ngx_http_51D_main_conf_t;

/**
 * Module server config.
 */
typedef struct {
	ngx_http_51D_match_conf_t matchConf; /**< The match to carry out in this
	                                        server's locations. */
} ngx_http_51D_srv_conf_t;

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
 * Get a fiftyoneDegreesConfigHash instance based on the main configuration
 * @param fdmcf main configuration
 * @return fiftyoneDegreesConfigHash instance
 */
static ConfigHash
get_config_hash(ngx_http_51D_main_conf_t *fdmcf) {
	ConfigHash config;
	// Determine performance profile
	switch (fdmcf->performanceProfile) {
		case ngx_http_51D_in_memory:
			config = HashInMemoryConfig;
			break;
		// Only support the in_memory performance profile
		// as Nginx is meant to be used in a high performance
		// environment.
		//
		// case ngx_http_51D_high_performance:
		// 	config = HashHighPerformanceConfig;
		// 	break;
		// case ngx_http_51D_low_memory:
		// 	config = HashLowMemoryConfig;
		// 	break;
		// case ngx_http_51D_balanced:
		// 	config = HashBalancedConfig;
		// 	break;
		// case ngx_http_51D_balanced_temp:
		// 	config = HashBalancedTempConfig;
		// 	break;
		default:
			// Default to the in_memory profile
			config = HashInMemoryConfig;
			break;
	}

	// Set drift value
	if (fdmcf->drift > 0 && fdmcf->drift != NGX_CONF_UNSET_UINT) {
		config.drift = fdmcf->drift;
	}

	// Set difference value
	if (fdmcf->difference > 0 && fdmcf->difference != NGX_CONF_UNSET_UINT) {
		config.difference = fdmcf->difference;
	}

	// Max concurrency is always set to the number of worker processes
	if (fdmcf->maxConcurrency != NGX_CONF_UNSET_UINT) {
		config.strings.concurrency = fdmcf->maxConcurrency;
		config.components.concurrency = fdmcf->maxConcurrency;
		config.maps.concurrency = fdmcf->maxConcurrency;
		config.properties.concurrency = fdmcf->maxConcurrency;
		config.values.concurrency = fdmcf->maxConcurrency;
		config.profiles.concurrency = fdmcf->maxConcurrency;
		config.rootNodes.concurrency = fdmcf->maxConcurrency;
		config.nodes.concurrency = fdmcf->maxConcurrency;
		config.profileOffsets.concurrency = fdmcf->maxConcurrency;
	}

	// Set allow unmatched
	if (fdmcf->allowUnmatched != NGX_CONF_UNSET_UINT) {
		config.b.allowUnmatched = fdmcf->allowUnmatched == 1 ? true : false;
	}

	// Set use performance graph
	if (fdmcf->usePerformanceGraph != NGX_CONF_UNSET_UINT) {
		config.usePerformanceGraph = 
			fdmcf->usePerformanceGraph == 1 ? true : false;
	}

	// Set use predictive graph
	if (fdmcf->usePredictiveGraph != NGX_CONF_UNSET_UINT) {	
		config.usePredictiveGraph =
			fdmcf->usePredictiveGraph == 1 ? true : false;
	}

	return config;
}

/**
 * Module post config. Adds the module to the HTTP access phase array, sets the
 * defaults if necessary, and sets the shared memory zones. Added header and
 * body filter to the filter chain for develiring Javascript content upon
 * requests.
 *
 * This will initialise the main configuration data, including the
 * performanceProfile, drift, difference ...
 *
 * @param cf nginx config.
 * @return ngx_int_t nginx conf status.
 */
static ngx_int_t
ngx_http_51D_post_conf(ngx_conf_t *cf)
{
	ngx_http_handler_pt *h;
	ngx_http_core_main_conf_t *cmcf;
	ngx_http_51D_main_conf_t *fdmcf;
	ngx_atomic_int_t tagOffset;
	ngx_str_t resourceManagerName;
	size_t size;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	fdmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_51D_module);

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

	*h = ngx_http_51D_handler;

	// set handler for response header and body
	ngx_http_next_header_filter = ngx_http_top_header_filter;
	ngx_http_top_header_filter = ngx_http_51D_header_filter;
	ngx_http_next_body_filter = ngx_http_top_body_filter;
	ngx_http_top_body_filter = ngx_http_51D_body_filter;

	// Set the default value separator if necessary.
	if ((int)fdmcf->valueSeparator.len <= 0) {
		fdmcf->valueSeparator.data = FIFTYONE_DEGREES_VALUE_SEPARATOR;
		fdmcf->valueSeparator.len = ngx_strlen(fdmcf->valueSeparator.data);
	}

	// Check if data file if necessary.
	if ((int)fdmcf->dataFile.len <= 0) {
		ngx_conf_log_error(
			NGX_LOG_NOTICE,
			cf,
			0,
			"51Degrees no data file was set.");
		ngx_log_error(
			NGX_LOG_INFO,
			cf->cycle->log,
			0,
			"51D_file_path was not set. No resource was loaded.");
		return NGX_OK;
	}

	// Initialise the shared memory zone for the resource manager.
	resourceManagerName.data = (u_char *) "51Degrees Shared Resource Manager";
	resourceManagerName.len = ngx_strlen(resourceManagerName.data);
	// By increasing the tag each time, the shared memory zone won't be reused.
	// Thus, during a reload, the old allocated resources in the shared memory
	// will be freed automatically.
	tagOffset = ngx_atomic_fetch_add(&ngx_http_51D_shm_tag, (ngx_atomic_int_t)1);

	// Need to get the size of memory that the resource manager will occupy.
	ConfigHash config = get_config_hash(fdmcf);
	PropertiesRequired properties = PropertiesDefault;

	EXCEPTION_CREATE
	size = fiftyoneDegreesHashSizeManagerFromFile(
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
	size *= FIFTYONE_DEGREES_MEMORY_ADJUSTMENT;

	ngx_http_51D_shm_resource_manager =
		ngx_shared_memory_add(
			cf,
			&resourceManagerName,
			size,
			&ngx_http_51D_module + tagOffset);
	ngx_http_51D_shm_resource_manager->init =
		ngx_http_51D_init_shm_resource_manager;
	return NGX_OK;
}

/**
 * Initialises the match config so no headers and body are set.
 * @param matchConf to initialise.
 */
static void
ngx_http_51D_init_match_conf(ngx_http_51D_match_conf_t *matchConf)
{
    matchConf->multiMask = 0;
	matchConf->setHeaders = NGX_CONF_UNSET_UINT;
	matchConf->headerCount = 0;
	matchConf->header = NULL;
	matchConf->body = NULL;
}

/**
 * Create main config. Allocates memory to the configuration and initialises
 * config options to -1 (unset).
 * @param cf nginx config.
 * @return Pointer to module main config.
 */
static void *
ngx_http_51D_create_main_conf(ngx_conf_t *cf)
{
	ngx_http_51D_main_conf_t *conf;
	ngx_core_conf_t *ccf =
		(ngx_core_conf_t *)ngx_get_conf(cf->cycle->conf_ctx, ngx_core_module);

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_51D_main_conf_t));
    if (conf == NULL) {
        return NULL;
    }

	// Initialise default value of main configuration
	// Enum field requires to be ngx_uint_t -1 as default
	conf->performanceProfile = NGX_CONF_UNSET_UINT;
	conf->drift = NGX_CONF_UNSET_UINT;
	conf->difference = NGX_CONF_UNSET_UINT;
	conf->maxConcurrency = ccf->worker_processes;
	conf->allowUnmatched = NGX_CONF_UNSET_UINT;
	conf->usePerformanceGraph = NGX_CONF_UNSET_UINT;
	conf->usePredictiveGraph = NGX_CONF_UNSET_UINT;

	// Init others
	memset(conf->properties, 0, FIFTYONE_DEGREES_MAX_STRING);
    conf->dataFile = (ngx_str_t)ngx_null_string;
	conf->results = NULL;
	memset(conf->valueString, 0, FIFTYONE_DEGREES_MAX_STRING);
	conf->resourceManager = NULL;
	conf->setRespHeaderCount = 0;
	conf->setRespHeader = NULL;
	conf->valueSeparator = (ngx_str_t)ngx_null_string;
		
	ngx_http_51D_init_match_conf(&conf->matchConf);
    return conf;
}

/**
 * Create location config. Allocates memory to the configuration.
 * @param cf nginx config.
 * @return Pointer to module location config.
 */
static void *
ngx_http_51D_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_51D_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_51D_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }
    ngx_http_51D_init_match_conf(&conf->matchConf);
	return conf;
}

/**
 * Create server config. Allocates memory to the configuration.
 * @param cf nginx config.
 * @return Pointer to the server config.
 */
static void *
ngx_http_51D_create_srv_conf(ngx_conf_t *cf)
{
	ngx_http_51D_srv_conf_t *conf;

	conf = ngx_palloc(cf->pool, sizeof(ngx_http_51D_srv_conf_t));
	if (conf == NULL) {
		return NULL;
	}
	ngx_http_51D_init_match_conf(&conf->matchConf);
	return conf;
}

/**
 * Merge location config. Either gets the value of count that is set, or sets
 * to the default of 0.
 */
static char *
ngx_http_51D_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_51D_loc_conf_t  *prev = parent;
	ngx_http_51D_loc_conf_t  *conf = child;

	ngx_conf_merge_uint_value(
		conf->matchConf.headerCount, prev->matchConf.headerCount, 0);

	ngx_conf_merge_uint_value(
		conf->matchConf.setHeaders, prev->matchConf.setHeaders, NGX_CONF_UNSET_UINT);

	return NGX_CONF_OK;
}

/**
 * Merge server config. Either gets the value of count that is set, or sets
 * to the default of 0.
 */
static char *
ngx_http_51D_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_51D_srv_conf_t  *prev = parent;
	ngx_http_51D_srv_conf_t  *conf = child;

	ngx_conf_merge_uint_value(
		conf->matchConf.headerCount, prev->matchConf.headerCount, 0);

	ngx_conf_merge_uint_value(
		conf->matchConf.setHeaders, prev->matchConf.setHeaders, NGX_CONF_UNSET_UINT);

	return NGX_CONF_OK;
}

/**
 * Shared memory alloc function. Replaces fiftyoneDegreesMalloc to store
 * the data set in the shared memory zone.
 * @param __size the size of memory to allocate.
 * @return void* a pointer to the allocated memory.
 */
void *ngx_http_51D_shm_alloc(size_t __size)
{
	void *ptr = NULL;
	ngx_slab_pool_t *shpool;
	shpool = (ngx_slab_pool_t *) ngx_http_51D_shm_resource_manager->shm.addr;
	ptr = ngx_slab_alloc_locked(shpool, __size);
	ngx_log_debug2(
		NGX_LOG_DEBUG_ALL,
		ngx_cycle->log,
		0,
		"51Degrees shm alloc %d %p",
		__size,
		ptr);
	if (ptr == NULL) {
		ngx_log_error(
			NGX_LOG_ERR,
			ngx_cycle->log,
			0,
			"51Degrees shm failed to allocate memory, not enough shared memory.");
	}
	return ptr;
}

/**
 * Shared memory alloc aligned function. If the __size is not mulitple of
 * alignment, it will be round up.
 *
 * The method in Device Detection C is only used to allocate
 * InterlockDoubleWidth whose size is 64bit on 32_bit system and 128 bit on
 * 64_bit system.
 *
 * The double width Interlock exchange requires allocated memory to be aligned
 * to work. Nginx shared memory does not provide a mechanism to perform memory
 * aligned allocation. However, without such API, normal ngx_slab_alloc should
 * still be sufficient because of the way that shared memory is allocated and
 * managed in Nginx.
 *
 * The shared memory is allocated by mmap() which will always map on page-aligned
 * address. The shared memory is then managed by ngx_slab_pool_t type which
 * divides all shared zone into pages. Each page is used for allocating objects
 * of the same size. Each ngx_slab_alloc require the specified size to be power
 * of 2 and has minimum size of 8 bytes (64_bit). Nonconforming values are rounded
 * up. Thus shared memory object will always be aligned on 64 bit boundary at least.
 * Since the alignment of InterlockDoubleWidth object is at 32 bit on 32_bit system
 * and 64 bit on 64 bit system, it will fit nicely on shared memory layout and is
 * aligned properly.
 *
 * P/s: How virtual memory address maintains alignment with physical memory address?
 * It is because virtual memory address is combination of pages whose size is power
 * of 2 (512 to 8192 with 4096 is typical), so when being mapped on physical memory
 * the alignment will always be maintained. Additionally natural alignment of 32 bit
 * system is 32 bit (*16=512) and 64 (*8=512) for 64 bit system, so when combined
 * with strict minimum memory size of 8 bytes in Nginx shared memory slab model, the
 * double width alignment will be preserved.
 *
 * @param alignment of the requested memory block
 * @param __size to be allocated
 * @return pointer to the allocated memory
 */
void *ngx_http_51D_shm_alloc_aligned(int alignment, size_t __size) {
	size_t actualAllocSize =
		__size % alignment ? (__size / alignment + 1) * alignment : __size;
	return ngx_http_51D_shm_alloc(actualAllocSize);
}

/**
 * Shared memory free function. Replaces fiftyoneDegreesFree to free pointers
 * to the shared memory zone.
 * @param __ptr pointer to the memory to be freed.
 */
void ngx_http_51D_shm_free(void *__ptr)
{
	ngx_slab_pool_t *shpool;
	shpool = (ngx_slab_pool_t *) ngx_http_51D_shm_resource_manager->shm.addr;
	if ((u_char *) __ptr < shpool->start || (u_char *) __ptr > shpool->end) {
		// The memory is not in the shared memory pool, so free with standard
		// free function.
		ngx_log_debug1(
			NGX_LOG_DEBUG_ALL,
			ngx_cycle->log,
			0,
			"51Degrees shm free (non shared) %p",
			__ptr);
		free(__ptr);
	}
	else {
		ngx_log_debug1(
			NGX_LOG_DEBUG_ALL,
			ngx_cycle->log,
			0,
			"51Degrees shm free %p",
			__ptr);
		ngx_slab_free_locked(shpool, __ptr);
	}
}

/**
 * Init resource manager memory zone. Allocates space for the resource manager
 * in the shared memory zone.
 * @param shm_zone the shared memory zone.
 * @param data if the zone has been carried over from a reload, this is the
 *        old data.
 * @return ngx_int_t nginx conf status.
 */
static ngx_int_t
ngx_http_51D_init_shm_resource_manager(ngx_shm_zone_t *shm_zone, void *data)
{
	ngx_slab_pool_t *shpool;
	ResourceManager *resourceManager;
	shpool = (ngx_slab_pool_t *) ngx_http_51D_shm_resource_manager->shm.addr;

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
			"51Degrees shared memory could not be allocated.");
		return NGX_ERROR;
	}
	ngx_log_debug1(
		NGX_LOG_DEBUG_ALL,
		shm_zone->shm.log,
		0,
		"51Degrees initialised shared memory with size %d.",
		shm_zone->shm.size);
	
	return NGX_OK;
}

/**
 * Init module function. Initialises the resrouce manager with the given
 * initialisation parameters. Throws an error if the resource manager could
 * not be initialised.
 * @param cycle the current nginx cycle.
 * @return ngx_int_t nginx conf status.
 */
static ngx_int_t
ngx_http_51D_init_module(ngx_cycle_t *cycle)
{
	ngx_http_51D_main_conf_t *fdmcf;
	ngx_slab_pool_t *shpool;

	// Get module main config.
	fdmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_51D_module);

	if (ngx_http_51D_shm_resource_manager == NULL) {
		return NGX_OK;
	}
	fdmcf->resourceManager =
		(ResourceManager *)ngx_http_51D_shm_resource_manager->data;

	shpool = (ngx_slab_pool_t *)ngx_http_51D_shm_resource_manager->shm.addr;

	// Set the memory allocation function to use the shared memory zone.
	Malloc = ngx_http_51D_shm_alloc;
	Free = ngx_http_51D_shm_free;
	MallocAligned = ngx_http_51D_shm_alloc_aligned;
	FreeAligned = ngx_http_51D_shm_free;

	// Initialise the resource manager.
	ngx_shmtx_lock(&shpool->mutex);
	ngx_http_51D_worker_count =
		(ngx_atomic_t *)ngx_http_51D_shm_alloc(sizeof(ngx_atomic_t));
	ngx_atomic_cmp_set(ngx_http_51D_worker_count, 0, 0);

	// Need to determine the ConfigHash at this point
	ConfigHash config = get_config_hash(fdmcf);
	PropertiesRequired properties = PropertiesDefault;

	EXCEPTION_CREATE;
	HashInitManagerFromFile(
		fdmcf->resourceManager,
		&config,
		&properties,
		(const char *)fdmcf->dataFile.data,
		exception
	);
	// Release lock
	ngx_shmtx_unlock(&shpool->mutex);

	if (EXCEPTION_FAILED) {
		return report_status(
			cycle->log,
			exception->status,
			(const char *)fdmcf->dataFile.data);
	}
	// Reset the malloc and free functions as nothing else should be allocated
	// in the shared memory zone.
	Malloc = MemoryStandardMalloc;
	Free = MemoryStandardFree;
	MallocAligned = MemoryStandardMallocAligned;
	FreeAligned = MemoryStandardFreeAligned;

	return NGX_OK;
}

/**
 * Performance profile map, to map user specified string with corresponding
 * profile.
 */
static ngx_conf_enum_t ngx_http_51D_performance_profiles[] = {
	{ ngx_string("IN_MEMORY"), ngx_http_51D_in_memory },
	{ ngx_string("HIGH_PERFORMANCE"), ngx_http_51D_high_performance },
	{ ngx_string("LOW_MEMORY"), ngx_http_51D_low_memory },
	{ ngx_string("BALANCED"), ngx_http_51D_balanced },
	{ ngx_string("BALANCED_TEMP"), ngx_http_51D_balanced_temp },
	{ ngx_string("DEFAULT"), ngx_http_51D_balanced },
	{ ngx_null_string, 0 }
};

/**
 * Definitions of functions which can be called from the config file.
 * --51D_match_single takes two string arguments, the name of the header
 * to be set and a comma separated list of properties to be returned. The thrid
 * argument is optional to specify the query argument to be used over the
 * default User-Agent. Is called within main, server and location block.
 * Enables User-Agent matching. Deprecated in favor of 51D_match_ua.
 * --51D_match_ua takes two string arguments, the name of the header
 * to be set and a comma separated list of properties to be returned. The thrid
 * argument is optional to specify the query argument to be used over the
 * default User-Agent. Is called within main, server and location block.
 * Enables User-Agent matching.
 * --51D_match_all takes two string arguments, the name of the header to
 * be set and a comma separated list of properties to be returned. Is
 * called within main, server and location block. Enables multiple http header
 * matching.
 * --51D_get_javascript_single takes one string argument, the Javascript
 * property to be returned. The second argument is optional to specify the
 * query argument to be used over the User-Agent header. Is only called within
 * the location block. This enables User-Agent matching to obtain the required
 * Javascript property.
 * --51D_get_javascript_all takes one string argument, the Javascript property
 * to be returned. Is only called within the location block. This enables 
 * multiple http header matching.
 * --51D_performance_profile takes one enum argument, the performance profile
 * to set. Values can be "IN_MEMORY", "HIGH_PERFORMANCE", "LOW_MEMORY",
 * "BALANCED", "BALANCED_TEMP", "DEFAULT". Currently only "IN_MEMORY" is
 * supported.
 * --51D_drift takes one integer argument, the drift value to set
 * --51D_difference takes one integer argument, the difference value to set
 * --51D_max_concurrency takes one integer argument, the maximum number of
 * concurrency that the engine can be run with. In this case, it will be
 * the number of worker processes.
 * --51D_allow_unmatched takes one enum argument, which specifies whether
 * unmatched result is allowed or not. This enum represents a boolean
 * value. This is to be consistent with other integration such as Varnish.
 * The values can be "on" or "off".
 * --51D_use_performance_graph takes one enum argument, which specifies
 * whether performance graph should be used. This enum represents a boolean
 * value.
 * --51D_use_predictive_graph takes one enum argument, which specifies
 * whether predictive graph should be used. This enum represents a boolean
 * value. 
 * --51D_file_path takes one string argument, the path to a
 * 51Degrees data file. Is called within server block.
 * --51D_value_separator takes one string argument, the separator of the
 * values being returned.
 */
static ngx_command_t  ngx_http_51D_commands[] = {

	{ ngx_string("51D_match_single"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_loc,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_single"),
	NGX_HTTP_SRV_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_srv,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_single"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_main,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_ua"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_loc,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_ua"),
	NGX_HTTP_SRV_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_srv,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_ua"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_main,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_client_hints"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_loc,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_client_hints"),
	NGX_HTTP_SRV_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_srv,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_client_hints"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE23,
	ngx_http_51D_set_main,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_all"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
	ngx_http_51D_set_loc,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_all"),
	NGX_HTTP_SRV_CONF|NGX_CONF_TAKE2,
	ngx_http_51D_set_srv,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_match_all"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE2,
	ngx_http_51D_set_main,
	NGX_HTTP_LOC_CONF_OFFSET,
    0,
	NULL },

	{ ngx_string("51D_get_javascript_single"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
	ngx_http_51D_set_loc_cdn,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	{ ngx_string("51D_get_javascript_all"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	ngx_http_51D_set_loc_cdn,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	{ ngx_string("51D_set_resp_headers"),
	NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
	ngx_http_51D_set_loc_resp,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	{ ngx_string("51D_set_resp_headers"),
	NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
	ngx_http_51D_set_srv_resp,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	{ ngx_string("51D_set_resp_headers"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_http_51D_set_main_resp,
	NGX_HTTP_LOC_CONF_OFFSET,
	0,
	NULL },

	{ ngx_string("51D_performance_profile"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_enum_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_main_conf_t, performanceProfile),
	&ngx_http_51D_performance_profiles },

	{ ngx_string("51D_drift"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_num_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_main_conf_t, drift),
	NULL },

	{ ngx_string("51D_difference"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_num_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_main_conf_t, difference),
	NULL },

	{ ngx_string("51D_allow_unmatched"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_flag_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_main_conf_t, allowUnmatched),
	NULL },

	{ ngx_string("51D_use_performance_graph"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_flag_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_main_conf_t, usePerformanceGraph),
	NULL },

	{ ngx_string("51D_use_predictive_graph"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_flag_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_main_conf_t, usePredictiveGraph),
	NULL },

	{ ngx_string("51D_file_path"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_str_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_main_conf_t, dataFile),
	NULL },

	{ ngx_string("51D_value_separator"),
	NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
	ngx_conf_set_str_slot,
	NGX_HTTP_MAIN_CONF_OFFSET,
	offsetof(ngx_http_51D_main_conf_t, valueSeparator),
	NULL },

	ngx_null_command
};

/**
 * Module context. Sets the configuration functions.
 */
static ngx_http_module_t ngx_http_51D_module_ctx = {
	NULL,                          /* preconfiguration */
	ngx_http_51D_post_conf,        /* postconfiguration */

	ngx_http_51D_create_main_conf, /* create main configuration */
	NULL,                          /* init main configuration */

	ngx_http_51D_create_srv_conf,  /* create server configuration */
	ngx_http_51D_merge_srv_conf,   /* merge server configuration */

	ngx_http_51D_create_loc_conf,  /* create location configuration */
	ngx_http_51D_merge_loc_conf,   /* merge location configuration */
};

/**
 * Skip the component name in the header and return the pointer
 * to the response header name.
 * @param headerName name of the SetHeader property, starting from
 * the component part.
 * @return a pointer to the header name to set.
 */
static const char *skipComponentName(const char *headerName) {
	// Skip the first uppercase character.
	headerName++;
	while (headerName[0] >= 'a' && headerName[0] <= 'z') {
		headerName++;
	}
	return headerName;
}

/**
 * Get the header index of a given header name in the list of
 * detected headers.
 * @param headerNames list of detected headers.
 * @param headerName a header name to search for.
 * @param respHeaderCount the number of detected headers.
 * @return index of the input header. -1 if not found.
 */
static ngx_int_t getRespHeaderIndex(
	ngx_str_t *headerNames,
	const char *headerName,
	ngx_uint_t respHeaderCount) {
	ngx_uint_t i;
	for (i = 0; i < respHeaderCount; i++) {
		if (ngx_strcmp(headerNames[i].data,
			headerName) == 0) {
			return i;
		}
	}
	return -1;
}

/**
 * Find a header name in a header to set linked list.
 * @param respHeader pointer to the first element in a list
 * @param headerName a header to search for.
 * @return pointer to the element that match. NULL if not found.
 */
static ngx_http_51D_data_to_set *getRespHeader(
	ngx_http_51D_data_to_set *respHeader,
	const char *headerName) {
	ngx_http_51D_data_to_set *header = respHeader;
	while (header != NULL) {
		if (ngx_strcmp(headerName, header->headerName.data) == 0) {
			return header;
		}
	}
	return NULL;
}

/**
 * Initialise the array of response headers to set.
 * @param cycle pointer to the current cycle.
 * @param fdmcf pointer to the 51Degrees main config object.
 * @param dataSet pointer to the 51Degrees Hash dataset.
 * @return status.
 */
static ngx_int_t
initRespHeaders(
	ngx_cycle_t *cycle,
	ngx_http_51D_main_conf_t *fdmcf,
	DataSetHash *dataSet) {
	ngx_http_51D_data_to_set **respHeaderPtr;
	ngx_uint_t i, respHeaderCount = 0;
	ngx_int_t headerIndex;
	const char *propertyName, *headerName;

	ngx_str_t *headerNames = (ngx_str_t *)ngx_palloc(
		cycle->pool, dataSet->b.b.available->count * sizeof(ngx_str_t));
	if (headerNames == NULL) {
		report_insufficient_memory_status(cycle->log);
		return NGX_ERROR;
	}

	ngx_uint_t *propertyCounts = (ngx_uint_t *)ngx_pcalloc(
		cycle->pool, dataSet->b.b.available->count * sizeof(ngx_uint_t));
	if (propertyCounts == NULL) {
		report_insufficient_memory_status(cycle->log);
		return NGX_ERROR;
	}

	// Count the set header properties
	for (i = 0; i <  dataSet->b.b.available->count; i++) {
		propertyName = STRING(dataSet->b.b.available->items[i].name.data.ptr);
		if (ngx_strncmp(
			FIFTYONE_DEGREES_SET_HEADER_PREFIX,
			propertyName,
			ngx_strlen(FIFTYONE_DEGREES_SET_HEADER_PREFIX)) == 0) {
			headerName = skipComponentName(
				propertyName + ngx_strlen(FIFTYONE_DEGREES_SET_HEADER_PREFIX));
			if ((headerIndex =
				getRespHeaderIndex(headerNames, headerName, respHeaderCount)) < 0) {
				headerNames[respHeaderCount].len = ngx_strlen(headerName);
				headerNames[respHeaderCount].data =
					(u_char *)ngx_palloc(cycle->pool, headerNames[respHeaderCount].len + 1);
				if (headerNames[respHeaderCount].data == NULL) {
					report_insufficient_memory_status(cycle->log);
					return NGX_ERROR;
				}

				strcpy((char *)headerNames[respHeaderCount].data, headerName);
				headerIndex = respHeaderCount;
				respHeaderCount++;
			}
			propertyCounts[headerIndex]++;
		}
	}

	// Allocate some space for the response headers to set.
	if (respHeaderCount > 0) {

		fdmcf->setRespHeaderCount = respHeaderCount;

		// Set the header names
		respHeaderPtr = &fdmcf->setRespHeader;
		for (i = 0; i < respHeaderCount; i++) {
			*respHeaderPtr = (ngx_http_51D_data_to_set *)ngx_palloc(
				cycle->pool, sizeof(ngx_http_51D_data_to_set));
			if (*respHeaderPtr == NULL) {
				report_insufficient_memory_status(cycle->log);
				return NGX_ERROR;
			}

			//  header has to be constructed
			// with all evidence, not just a User-Agent. User-Agent
			// might be deprecated in the future.
			(*respHeaderPtr)->multi = 1<<ngx_http_51D_all;
	
			// This doesn't need to be allocated in separate memmory
			// as it won't be freed from available properties.
			(*respHeaderPtr)->headerName = headerNames[i];
			(*respHeaderPtr)->lowerHeaderName.data =
				(u_char *)ngx_palloc(
					cycle->pool, 
					headerNames[i].len + 1);
			if ((*respHeaderPtr)->lowerHeaderName.data == NULL) {
				report_insufficient_memory_status(cycle->log);
				return NGX_ERROR;
			}
			
			ngx_strlow(
				(*respHeaderPtr)->lowerHeaderName.data,
				(*respHeaderPtr)->headerName.data,
				(*respHeaderPtr)->headerName.len);
			(*respHeaderPtr)->lowerHeaderName.len = (*respHeaderPtr)->headerName.len;

			// Set to zero so we know where we are while filling.
			(*respHeaderPtr)->propertyCount = 0;
			(*respHeaderPtr)->property =
				(ngx_str_t **)ngx_palloc(cycle->pool, propertyCounts[i] * sizeof(ngx_str_t *));
			if (fdmcf->setRespHeader[i].property == NULL) {
				report_insufficient_memory_status(cycle->log);
				return NGX_ERROR;
			}
			(*respHeaderPtr)->variableName = (ngx_str_t)ngx_null_string;
			(*respHeaderPtr)->next = NULL;
			respHeaderPtr = &(*respHeaderPtr)->next;
		}

		// Fill the properties.
		ngx_http_51D_data_to_set *respHeader;
		for (i = 0;  i < dataSet->b.b.available->count; i++) {
			propertyName = STRING(dataSet->b.b.available->items[i].name.data.ptr);
			if (ngx_strncmp(
				FIFTYONE_DEGREES_SET_HEADER_PREFIX,
				propertyName,
				ngx_strlen(FIFTYONE_DEGREES_SET_HEADER_PREFIX)) == 0) {
				headerName = skipComponentName(
					propertyName + ngx_strlen(FIFTYONE_DEGREES_SET_HEADER_PREFIX));
				respHeader = getRespHeader(fdmcf->setRespHeader, headerName);
				respHeader->property[respHeader->propertyCount] =
					(ngx_str_t *)ngx_palloc(cycle->pool, sizeof(ngx_str_t));
				if (respHeader->property[respHeader->propertyCount] == NULL) {
					report_insufficient_memory_status(cycle->log);
					return NGX_ERROR;
				}

				// Add the property to the response header list
				size_t len = ngx_strlen(propertyName);
				respHeader->property[respHeader->propertyCount]->len = len;
				respHeader->property[respHeader->propertyCount]->data = 
					(u_char *)ngx_palloc(cycle->pool, len + 1);
				if (respHeader->property[respHeader->propertyCount]->data == NULL) {
					report_insufficient_memory_status(cycle->log);
					return NGX_ERROR;
				}

				strcpy((char *)respHeader->property[respHeader->propertyCount]->data,
					propertyName);
				respHeader->propertyCount++;
			}
		}
	}
	return NGX_OK;
}

/**
 * Init process function. Creates a result set from the shared resource
 * manager.
 * This result set is local to the process.
 * Here is where the ResultsHash will be initialised.
 * @param cycle the current nginx cycle.
 * @return ngx_int_t nginx status.
 */
static ngx_int_t
ngx_http_51D_init_process(ngx_cycle_t *cycle)
{
	ngx_int_t status;
	ngx_http_51D_main_conf_t *fdmcf;

	if (ngx_http_51D_shm_resource_manager == NULL) {
		return NGX_OK;
	}
	fdmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_51D_module);
	DataSetHash *dataSet = (DataSetHash *)DataSetGet(fdmcf->resourceManager);
	ngx_uint_t overridesCount =
		dataSet->b.b.overridable != NULL ? dataSet->b.b.overridable->count : 0;
	
	// There can only be one result per unique header, so the max number of results
	// is the number of unique headers.
	fdmcf->results = ResultsHashCreate(
		fdmcf->resourceManager,
		dataSet->b.b.uniqueHeaders->count,
		overridesCount);

	// Initialise the response headers array.
	if ((status = initRespHeaders(cycle, fdmcf, dataSet)) != NGX_OK) {
		return status;
	};

	// Release the dataset
	DataSetRelease((DataSetBase *)dataSet);

	if (fdmcf->results == NULL) {
		return report_insufficient_memory_status(cycle->log);
	}

	// Increment the workers which are using the dataset.
	ngx_atomic_fetch_add(ngx_http_51D_worker_count, 1);
	return NGX_OK;
}

/**
 * Exit process function. Frees the results set that was created on process
 * init.
 * @param cycle the current nginx cycle.
 */
void
ngx_http_51D_exit_process(ngx_cycle_t *cycle)
{
	ngx_http_51D_main_conf_t *fdmcf;

	if (ngx_http_51D_shm_resource_manager == NULL) {
		return;
	}

	fdmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_51D_module);

	ResultsHashFree(fdmcf->results);

	// Decrement the worker count. Try 5 times if not succeed.
	ngx_uint_t i;
	for (
		i = 5;
		i > 0 && ngx_atomic_fetch_add(ngx_http_51D_worker_count, -1) != 1;
		i--) {}
}

/**
 * Exit master process. Frees resource created for the 51Degrees module.
 * @param cycle the current nginx cycle.
 */
void
ngx_http_51D_exit_master(ngx_cycle_t *cycle)
{
	ngx_slab_pool_t *shpool;
	ResourceManager *resourceManager;
	if (ngx_http_51D_shm_resource_manager == NULL) {
		return;
	}

	resourceManager =
		(ResourceManager *)ngx_http_51D_shm_resource_manager->data;
	shpool = (ngx_slab_pool_t *)ngx_http_51D_shm_resource_manager->shm.addr;

	// Lock the shared memory and free any memory allocated for the module
	ngx_shmtx_lock(&shpool->mutex);
	if (*ngx_http_51D_worker_count > 0) {
		ngx_log_error(
			NGX_LOG_WARN,
			cycle->log,
			0,
			"51Degrees not all child processes has terminated at master process termination");
	}

	// All child process should have already been terminated at this point.
	// Also once the master has terminated, any running worker process should
	// not be of any use so proceed to free the resource.
	Free = ngx_http_51D_shm_free;
	FreeAligned = ngx_http_51D_shm_free;
	ResourceManagerFree(resourceManager);
	Free = MemoryStandardFree; 
	FreeAligned = MemoryStandardFreeAligned;
	ngx_http_51D_shm_free((void *)resourceManager);
	ngx_http_51D_shm_free((void *)ngx_http_51D_worker_count);
	ngx_shmtx_unlock(&shpool->mutex);
}

/**
 * Module definition. Set the module context, commands, type and init function.
 */
ngx_module_t ngx_http_51D_module = {
	NGX_MODULE_V1,
	&ngx_http_51D_module_ctx,      /* module context */
	ngx_http_51D_commands,         /* module directives */
	NGX_HTTP_MODULE,               /* module type */
	NULL,                          /* init master */
	ngx_http_51D_init_module,      /* init module */
	ngx_http_51D_init_process,     /* init process */
	NULL,                          /* init thread */
	NULL,                          /* exit thread */
	ngx_http_51D_exit_process,     /* exit process */
	ngx_http_51D_exit_master,      /* exit master */
	NGX_MODULE_V1_PADDING
};

/**
 * Search headers function. Searched request headers for the name supplied.
 * Used when matching multiple http headers to find important headers.
 * See:
 * https://www.nginx.com/resources/wiki/start/topics/examples/headers_management
 */
static ngx_table_elt_t *
search_headers_in(ngx_http_request_t *r, u_char *name, size_t len) {
    ngx_list_part_t            *part;
    ngx_table_elt_t            *h;
    ngx_uint_t                  i;

    part = &r->headers_in.headers.part;
    h = part->elts;

    for (i = 0; /* void */ ; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (len != h[i].key.len || ngx_strcasecmp(name, h[i].key.data) != 0) {
            continue;
        }

        return &h[i];
    }

    return NULL;
}

/**
 * Add value function. Appends a string to a list separated by the
 * delimiter specified with 51D_valueSeparator, or a comma by default.
 * @param delimiter to split values with.
 * @param val the string to add to dst.
 * @param dst the string to append the val to.
 * @param length the size remaining to append val to dst.
 */
static void add_value(char *delimiter, char *val, char *dst, size_t length)
{
	// If the buffer already contains characters, append a pipe.
	if (dst[0] != '\0') {
		strncat(dst, delimiter, length);
		length -= strlen(delimiter);
	}

    // Append the value.
    strncat(dst, val, length);
}

/**
 * Check if a property being requested is a meta data property.
 * @param val the string value.
 */
static bool is_metadata(char *val) {
	return StringCompare(val, "Iterations") == 0 ||
		StringCompare(val, "Drift") == 0 ||
		StringCompare(val, "Difference") == 0 ||
		StringCompare(val, "Method") == 0 ||
		StringCompare(val, "UserAgents") == 0 ||
		StringCompare(val, "MatchedNodes") == 0 ||
		StringCompare(val, "DeviceId") == 0;
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
 * Search and get the evidence from the query string.
 * @param r the nginx http request
 * @param variableName the variable name to search for
 * @return the evidence
 */
static ngx_str_t *
get_evidence_from_query_string_base(ngx_http_request_t *r, ngx_str_t *variableName) {
	u_char *src, *dst;
	ngx_uint_t variableNameHash;
	ngx_variable_value_t *variable;
	ngx_str_t *evidence;

	variableNameHash = ngx_hash_strlow(variableName->data,
		variableName->data,
		variableName->len);
	variable = ngx_http_get_variable(r, variableName, variableNameHash);
	if (variable->not_found == 0 && (int)variable->len > 0) {
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
 * Search and get the evidence from the query string. If the variable does not
 * contains 'arg_' prefix, add it as this is required by nginx.
 * @param r the nginx http request
 * @param variableName the variable name to search for
 * @return the evidence
 */
static ngx_str_t *
get_evidence_from_query_string(ngx_http_request_t *r, ngx_str_t *variableName) {
	u_char *src, *dst;
	ngx_uint_t variableNameHash;
	ngx_variable_value_t *variable;
	ngx_str_t *evidence;
	ngx_str_t queryVarName;

	// Check if the variable contains the prefix 'arg_' required for query
	// string arguments
	if (ngx_strstr((char *)variableName->data, "arg_") !=
			(char *)variableName->data) {
		queryVarName.len = ngx_strlen("arg_") + variableName->len;
		queryVarName.data = (u_char *)ngx_palloc(r->pool, queryVarName.len + 1);
		if (queryVarName.data == NULL) {
			report_insufficient_memory_status(r->connection->log);
			return NULL;
		}
		if (sprintf((char *)queryVarName.data,
			"arg_%s",
			variableName->data) < 0) {
			ngx_log_error(
				NGX_LOG_ERR,
				r->connection->log,
				0,
				"51Degrees failed to compose query argument string.");
			return NULL; 
		};
	}
	else {
		queryVarName = *variableName;
	}

	return get_evidence_from_query_string_base(r, &queryVarName);
}

/**
 * Check whether there is a cookie with the name supplied.
 * @param r pointer to a http request
 * @param s name of the cookie to look for
 * @param cookie pointer to an ngx_str_t to write the value to
 * @return true if the cookie was found
 */
static bool has_cookie_value(
	ngx_http_request_t *r,
	ngx_str_t *s,
	ngx_str_t *cookie) {
#if nginx_version >= 1023002
	return ngx_http_parse_multi_header_lines(r, r->headers_in.cookie, s, cookie)
		!= NULL;
#else
	return ngx_http_parse_multi_header_lines(&r->headers_in.cookies, s, cookie)
		!= NGX_DECLINED;
#endif
}

/**
 * Add evidence required by the Hash device detection from cookie and query
 * string.
 * @param r pointer to a http request
 * @param results pointers to results hash instance
 * @param evidence pointer to the evidence collection
 * @param cookies pointer to the array of cookies
 */
static void
add_override_evidence_from_cookie_and_query(
	ngx_http_request_t *r,
	ResultsHash *results,
	EvidenceKeyValuePairArray *evidence) {
	ngx_uint_t i;
	ngx_str_t s, cookie;
	OverrideProperty property;
	String *fdString;
	DataSetHash *dataSet =
		(DataSetHash *)results->b.b.dataSet;
	if (dataSet->b.b.overridable != NULL) {
		for (i = 0; i < dataSet->b.b.overridable->count; i++) {
			property = dataSet->b.b.overridable->items[i];
			fdString = (String *)property.available->name.data.ptr;
			s.len = fdString->size - 1;
			if (dataSet->b.b.overridable->prefix) {
				s.len += ngx_strlen("51D_");
				s.data = (u_char *)ngx_palloc(r->pool, s.len + 1);
				if (s.data == NULL) {
					report_insufficient_memory_status(r->connection->log);
					break;
				}

				if (sprintf((char *)s.data, "51D_%s", &fdString->value) < 0) {
					ngx_log_error(
						NGX_LOG_ERR,
						r->connection->log,
						0,
						"51Degrees failed to compose evidence key string.");
					break;
				}
			}
			else {
				s.data = (u_char *)&fdString->value;
			}

			// Find evidence in cookie
			if (has_cookie_value(r, &s, &cookie) == true) {
				EvidenceAddString(
					evidence,
					FIFTYONE_DEGREES_EVIDENCE_COOKIE,
					(const char *)s.data,
					(const char *)cookie.data);
			}

			// Find evidence in query string
			ngx_str_t *queryEvidence = get_evidence_from_query_string(r, &s);
			if (queryEvidence != NULL && queryEvidence->len > 0) {
				EvidenceAddString(
					evidence,
					FIFTYONE_DEGREES_EVIDENCE_QUERY,
					(const char *)s.data,
					(const char *)queryEvidence->data);
			}
		}
	}
}

/**
 * Create an evidence array and added the evidence from the http request.
 * This should consider the evidence sent from cookies and query, since
 * overrides are required by customer.
 * This requires parsing the query string for the override values. For MVP
 * might consider the override from cookies only.
 * @param results the results to hold the return value of the detection
 * @param r the http request that contains the evidence
 * @param multiMode describes which headers to use for evidence.
 * @return an array of evidence
 */
static EvidenceKeyValuePairArray *get_evidence(
	ResultsHash *results,
	ngx_http_request_t *r,
	ngx_http_51D_multi_header_mode multiMode) {
	DataSetHash *dataSet =
		(DataSetHash *)results->b.b.dataSet;
	ngx_table_elt_t *searchResult;
	ngx_str_t *queryEvidence;
	// Calculate the size to allocate. 2 for overrides from cookies and query.
	// 2 from the header and query.
	size_t size = dataSet->b.b.uniqueHeaders->count * 2;
	if (dataSet->b.b.overridable != NULL) {
		size += dataSet->b.b.overridable->count * 2;
	}

	EvidenceKeyValuePairArray *evidence =
		EvidenceCreate(size);
	if (evidence != NULL) {
		// Create the evidence from the http headers
		ngx_uint_t i;
		FILE *f = fopen("_FILTERING.txt", "a");
		fprintf(f, "Multi Mode: '%d'\n", (int)multiMode);
		for (i = 0; i < dataSet->b.b.uniqueHeaders->count; i++) {
			const char *headerName =
				dataSet->b.b.uniqueHeaders->items[i].name;
			fprintf(f, "\n- Next header: '%s'", headerName);
			if ((multiMode & (1<<ngx_http_51D_client_hints)) 
				&& !(strncmp(headerName, "Sec-CH-", 7) == 0
					 || strncmp(headerName, "User-Agent", 10) == 0))
			{
				fprintf(f, " --- SKIPPED!");
				continue;
			}
			searchResult =
				search_headers_in(
					r, (u_char *)headerName, ngx_strlen(headerName));
			if (searchResult) {
				EvidenceAddString(
					evidence,
					FIFTYONE_DEGREES_EVIDENCE_HTTP_HEADER_STRING,
					headerName,
					(const char *)searchResult->value.data);
			}

			// Find evidence in query string
			ngx_str_t ngxHeaderName;
			ngxHeaderName.len = ngx_strlen(headerName);
			ngxHeaderName.data =
				(u_char *)ngx_palloc(r->pool, ngxHeaderName.len + 1);
			if (ngxHeaderName.data == NULL) {
				report_insufficient_memory_status(r->connection->log);
				return evidence;
			}

			strcpy((char *)ngxHeaderName.data, headerName);
			queryEvidence = get_evidence_from_query_string(r, &ngxHeaderName);
			if (queryEvidence != NULL && queryEvidence->len > 0) {
				EvidenceAddString(
					evidence,
					FIFTYONE_DEGREES_EVIDENCE_QUERY,
					headerName,
					(const char *)queryEvidence->data);	
			}
		}
		fprintf(f, "\n----- ----- -----\n");
		fclose(f);

		add_override_evidence_from_cookie_and_query(
			r, results, evidence);
	}
	else {
		report_insufficient_memory_status(r->connection->log);
	}

	return evidence;
}

/**
 * Get match function. Gets a match for either a single User-Agent or 
 * all request headers.
 *
 * @param fdmcf module main config.
 * @param r the current HTTP request.
 * @param multi see `ngx_http_51D_multi_header_mode`
 * @param userAgent pointer to the user agent to perform the match on.
 * @return Nginx status code.
 */
static ngx_uint_t ngx_http_51D_get_match(
	ngx_http_51D_main_conf_t *fdmcf,
	ngx_http_request_t *r,
	ngx_http_51D_multi_header_mode multi,
	ngx_str_t *userAgent)
{
	ResultsHash *results = fdmcf->results;
	
	FILE *f = fopen("_FILTERING.txt", "a");
	fprintf(f, "multi: '%d'\n", (int)multi);
	fclose(f);
	
	EXCEPTION_CREATE
	// If single requested, match for single User-Agent.
	if (multi & (1<<ngx_http_51D_ua_only))  {
		ResultsHashFromUserAgent(
			results,
			(const char *)userAgent->data,
			userAgent->len,
			exception);
		if (EXCEPTION_FAILED) {
			return report_status(
				r->connection->log,
				exception->status,
				(const char *)fdmcf->dataFile.data);
		}
	}
	else if (multi 
			 & ((1<<ngx_http_51D_multi_header_mode_count) - 1)
			 & ~(1<<ngx_http_51D_ua_only))
	{
		// Reset overrides array as we want a free
		// detection per request.
		fiftyoneDegreesOverrideValuesReset(results->b.overrides);

		EvidenceKeyValuePairArray *evidence =
			get_evidence(results, r, multi);
		if (evidence != NULL) {
			ResultsHashFromEvidence(
				results,
				evidence,
				exception);
			EvidenceFree(evidence);
			if (EXCEPTION_FAILED) {
				return report_status(
					r->connection->log,
					exception->status,
					(const char *)fdmcf->dataFile.data);
			}
		}
		else {
			return NGX_ERROR;
		}
	}
	return NGX_OK;
}

/**
 * Remove Unknown string from a given string and updated
 * the number of characers added with 'Unknown' being removed.
 * @param dest pointer to the string to be updated
 * @param charsAdded number of characters added to the string
 * that might contains Unknown. This will be updated after
 * 'Unknown' string is removed.
 */
static void removeUnknown(char *dest, ngx_int_t *charsAdded)
{
	const char *unknown = "Unknown";
	size_t unknownLen = ngx_strlen(unknown);	
	size_t destLen = ngx_strlen(dest);
	size_t memToMove = 0;
	dest = dest + (destLen - *charsAdded);

	char *unknownPos = NULL;
	while (*charsAdded > 0 &&
		(unknownPos = ngx_strstr(dest, unknown)) != NULL) {
		*charsAdded -= unknownLen;
		memToMove = ngx_strlen(unknownPos) - unknownLen;
		ngx_memmove(unknownPos, unknownPos + unknownLen, memToMove);
		unknownPos[memToMove] = '\0';
		dest = unknownPos;
	}
}

/**
 * Get value function. Gets the requested value for the current match and
 * appends the value to the list of values separated by the delimiter specified
 * with 51D_valueSeparator.
 * @param fdmcf 51Degrees module main config.
 * @param r the current HTTP request.
 * @param values_string the string to append the returned value to.
 * @param requiredPropertyName the name of the property to get the value for.
 * @param length the size allocated to the values_string variable.
 * @param includeNotAvailable whether or not, property whose hasValues=false or
 * value is 'Unknown' should be included as part of the valuesString. In all cases
 * whether properties are required by "51D_match_*" directives, this has to be 1
 * so that the values returned match the number of property required. Where this
 * is called by "51D_set_resp_headers", this is not required since they won't be
 * valid.
 * @param customSeparator a separator to use instead of the 51D_value_separator.
 * This is normally used for internal construction of string such as Client Hints
 * response headers list where separator has to be ','.
 */
void ngx_http_51D_get_value(
	ngx_http_51D_main_conf_t *fdmcf,
	ngx_http_request_t *r,
	char *values_string,
	const char *requiredPropertyName,
	size_t length,
	ngx_uint_t includeNotAvailable,
	const char *customSeparator) {
	size_t remainingLength = length - strlen(values_string);
	ResultsHash *results = fdmcf->results;
	DataSetHash *dataSet =
		(DataSetHash *)results->b.b.dataSet;
	const char *valueDelimiter;
	if (customSeparator == NULL) {
		valueDelimiter = (const char *)fdmcf->valueSeparator.data;
	}
	else {
		valueDelimiter = (const char *)customSeparator;
	}
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
		if (StringCompare(
			requiredPropertyName, "Iterations") == 0) {
			charsAdded = snprintf(
				dest, remainingLength, "%d", results->items->iterations);
		}
		else if (StringCompare(
			requiredPropertyName, "Drift") == 0) {
			charsAdded =
				snprintf(dest, remainingLength, "%d", results->items->drift);
		}
		else if (StringCompare(
			requiredPropertyName, "Difference") == 0) {
			charsAdded = snprintf(
				dest, remainingLength, "%d", results->items->difference);
		}
		else if (StringCompare(
			requiredPropertyName, "Method") == 0) {
			const char *method;
			switch (results->items->method) {
				case FIFTYONE_DEGREES_HASH_MATCH_METHOD_PERFORMANCE:
					method = "PERFORMANCE";
					break;
				case FIFTYONE_DEGREES_HASH_MATCH_METHOD_COMBINED:
					method = "COMBINED";
					break;
				case FIFTYONE_DEGREES_HASH_MATCH_METHOD_PREDICTIVE:
					method = "PREDICTIVE";
					break;
				case FIFTYONE_DEGREES_HASH_MATCH_METHOD_NONE:
				default:
					method = "NONE";
					break;
			}
			charsAdded = snprintf(dest, remainingLength, "%s", method);
		}
		else if (StringCompare(
			requiredPropertyName, "UserAgents") == 0) {
			charsAdded = snprintf(
				dest, remainingLength, "%s", results->items->b.matchedUserAgent);
		}
		else if (StringCompare(
			requiredPropertyName, "MatchedNodes") == 0) {
			charsAdded = snprintf(
				dest, remainingLength, "%d", results->items->matchedNodes);
		}
		else if (StringCompare(
			requiredPropertyName, "DeviceId") == 0) {
			char deviceId[40];
			HashGetDeviceIdFromResults(
				results,
				deviceId,
				sizeof(deviceId),
				exception);
			if (EXCEPTION_FAILED) {
				report_status(
					r->connection->log,
					exception->status,
					(const char *)fdmcf->dataFile.data);
				snprintf(dest, remainingLength, "%c", '\0');
				charsAdded = 0;
			}
			else {
				charsAdded = snprintf(dest, remainingLength, "%s", deviceId);
			}
		}
		else {
			int requiredPropertyIndex =
				PropertiesGetRequiredPropertyIndexFromName(
					dataSet->b.b.available, requiredPropertyName);

			bool hasValues = ResultsHashGetHasValues(
				results,
				requiredPropertyIndex,
				exception);
			if (hasValues) {
				charsAdded = ResultsHashGetValuesString(
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

				// Remove 'Unknown' string.
				if (!includeNotAvailable) {
					removeUnknown(dest, &charsAdded);
				}
			}
			else if (includeNotAvailable) {
				charsAdded = snprintf(
					dest, remainingLength,
					"%s",
					FIFTYONE_DEGREES_PROPERTY_NOT_AVAILABLE);
			}
		}
	}

	if (charsAdded < 0) {
		ngx_log_error(
			NGX_LOG_ERR,
			r->connection->log,
			0,
			"51Degrees failed to construct value string.");
	}
	else if (charsAdded > (ngx_int_t)remainingLength) {
		ngx_log_error(
			NGX_LOG_WARN,
			r->connection->log,
			0,
			"51Degrees value string is bigger than the available buffer.");
	}
}

/**
 * Get the user agent using the header name specified by the user in the
 * configuration. This is only applied to the single match.
 */
static ngx_str_t*
ngx_http_51D_get_user_agent(
	ngx_http_request_t *r,
	ngx_http_51D_data_to_set *data)
{
	ngx_str_t *userAgent;
	// Get the User-Agent from the request or variable.
	if (data != NULL && (int)data->variableName.len > 0) {
		userAgent = get_evidence_from_query_string_base(r, &data->variableName);
	}
	else if (r->headers_in.user_agent != NULL) {
		userAgent = &r->headers_in.user_agent[0].value;
	}
	else {
		userAgent = empty_string(r);
	}
	return userAgent;
}

/**
 * Perform detection if required and returned an escaped value string
 * for the header to set.
 * @param r a ngx_http_request_t.
 * @param fdmcf a main config object.
 * @param header a header to construct value string for.
 * @param haveMatch whether a match has already been perfomed for the
 * input User-Agents.
 * @param includeNotAvailable whether not available property (e.g. 'Unknown'
 * or hasValues=false) should be included in the value string.
 * @param userAgent a User-Agent string to perform the match on.
 * @param customSeparator custom separator to be used instead of the
 * 51D_value_separator.
 * @ return an escaped value string. NULL if error occurred.
 */
u_char *getEscapedMatchedValueString(
	ngx_http_request_t *r,
	ngx_http_51D_main_conf_t *fdmcf,
	ngx_http_51D_data_to_set *header,
	int haveMatch,
	ngx_uint_t includeNotAvailable,
	ngx_str_t *userAgent,
	const char *customSeparator) {
	memset(fdmcf->valueString, 0, FIFTYONE_DEGREES_MAX_STRING);

	// Get a match. If there are multiple instances of
	// 51D_match_single, 51D_match_ua, 51D_match_client_hints or 51D_match_all, then don't get the
	// match if it has already been fetched.
	if (haveMatch == 0) {
		ngx_uint_t ngxCode = 
			ngx_http_51D_get_match(fdmcf, r, header->multi, userAgent);
		if (ngxCode != NGX_OK) {
			return NULL;
		}	
	}

	// For each property, set the value in value_string_array.
	int property_index;
	for (property_index = 0;
		property_index < (int)header->propertyCount;
		property_index++) {
		ngx_http_51D_get_value(
			fdmcf,
			r,
			fdmcf->valueString,
			(const char *)header->property[property_index]->data,
			FIFTYONE_DEGREES_MAX_STRING,
			includeNotAvailable,
			customSeparator);
	}

	// Escape characters which cannot be added in a header value, most
	// importantly '\n' and '\r'.
	size_t valueStringLength = strlen(fdmcf->valueString);
	size_t escapedChars =
		(size_t)ngx_escape_json(NULL, (u_char *)fdmcf->valueString, valueStringLength);
	u_char *escapedValueString =
		(u_char *)ngx_palloc(
			r->pool, (valueStringLength + escapedChars + 1) * sizeof(char));
	if (escapedValueString == NULL) {
		report_insufficient_memory_status(r->connection->log);
		return NULL;
	}
	ngx_escape_json(escapedValueString, (u_char *)fdmcf->valueString, valueStringLength);
	escapedValueString[valueStringLength + escapedChars] = '\0';

	return escapedValueString;
}

/**
 * Process a request and perform device detection based on the request info.
 * This should perform a detection using either a User-Agent or all request
 * header.
 * @param r the http request
 * @param fdmcf the main configuration of 51Degrees module
 * @param header the header name to set
 * @param h the list of headers to store the match result
 * @param matchIndex the index in the headers list to store the match values
 * @param haveMatch indicate of a match has already been performed for input
 * userAgent
 * @param userAgent the user agent to perform the match for
 * @return code to indicate the status of the operation.
 */
ngx_uint_t
process(ngx_http_request_t *r,
		ngx_http_51D_main_conf_t *fdmcf,
		ngx_http_51D_data_to_set *header,
		ngx_table_elt_t *h[],
		int matchIndex,
		int haveMatch,
		ngx_str_t *userAgent) {
	u_char *escapedValueString = getEscapedMatchedValueString(
		r, fdmcf, header, haveMatch, 1, userAgent, NULL);
	if (escapedValueString == NULL) {
		return NGX_ERROR;
	}

	// For each property value pair, set a new header name and value.
	h[matchIndex] = ngx_list_push(&r->headers_in.headers);
	h[matchIndex]->key.data = (u_char *)header->headerName.data;
	h[matchIndex]->key.len = ngx_strlen(h[matchIndex]->key.data);
	h[matchIndex]->hash = ngx_hash_key(h[matchIndex]->key.data, h[matchIndex]->key.len);
	h[matchIndex]->value.data = escapedValueString;
	h[matchIndex]->value.len = ngx_strlen(h[matchIndex]->value.data);
	h[matchIndex]->lowcase_key = (u_char *)header->lowerHeaderName.data;
	return NGX_OK;
}

/**
 * Module handler, gets a match using either the User-Agent or multiple http
 * headers, then sets properties as headers.
 *
 * This will call the 'process' method.
 *
 * @param r the HTTP request.
 * @return ngx_int_t nginx status.
 */
static ngx_int_t
ngx_http_51D_handler(ngx_http_request_t *r)
{
	ngx_http_51D_main_conf_t *fdmcf;
	ngx_http_51D_srv_conf_t *fdscf;
	ngx_http_51D_loc_conf_t *fdlcf;
	ngx_http_51D_match_conf_t *matchConf[FIFTYONE_DEGREES_CONFIG_LEVELS];
	ngx_uint_t matchIndex = 0, rawMulti, haveMatch;
	ngx_http_51D_data_to_set *currentHeader;
	int totalHeaderCount, matchConfIndex;
	ngx_str_t *userAgent, *nextUserAgent;

	if (r->main->internal ||
		ngx_http_51D_shm_resource_manager == NULL ||
		r->headers_in.headers.last == NULL) {
		return NGX_DECLINED;
	}
	r->main->internal = 1;

	// Get 51Degrees configs.
	fdlcf = ngx_http_get_module_loc_conf(r, ngx_http_51D_module);
	fdscf = ngx_http_get_module_srv_conf(r, ngx_http_51D_module);
	fdmcf = ngx_http_get_module_main_conf(r, ngx_http_51D_module);

	matchConf[0] = &fdlcf->matchConf;
	matchConf[1] = &fdscf->matchConf;
	matchConf[2] = &fdmcf->matchConf;

	totalHeaderCount = matchConf[0]->headerCount +
					   matchConf[1]->headerCount +
					   matchConf[2]->headerCount;

	// C99 supports flexible array size.
	ngx_table_elt_t *h[totalHeaderCount];
	memset(h, 0, totalHeaderCount * sizeof(ngx_table_elt_t *));

	userAgent = ngx_http_51D_get_user_agent(r, NULL);

	// If no headers are specified in this location, tell nginx
	// this handler is done.
	if ((int)totalHeaderCount == 0) {
		return NGX_DECLINED;
	}
	// Look for single User-Agent matches, then multiple HTTP header
	// matches. Single and multi matches are done separately to reuse
	// a match instead of retrieving it multiple times. Start with
	// false (i.e. = 0) increase to true (i.e. = 1).
	for (rawMulti = ngx_http_51D_ua_only;
		rawMulti < ngx_http_51D_multi_header_mode_count;
		rawMulti++) {
		haveMatch = 0;

		// Go through the requested matches in location, server and
		// main configs.
		for (matchConfIndex = 0;
			matchConfIndex < FIFTYONE_DEGREES_CONFIG_LEVELS;
			matchConfIndex++) {
			// Loop over all match instances in the config.
			currentHeader = matchConf[matchConfIndex]->header;
			while (currentHeader != NULL) {
				// Process the match if it is the type we are looking for on
				// this pass.
				if ((currentHeader->multi & (1<<rawMulti)) &&
					(int)currentHeader->variableName.len <= 0 &&
					userAgent != NULL) {
					haveMatch = process(r, fdmcf, currentHeader, h, matchIndex, haveMatch, userAgent);
					matchIndex++;
				}
				// Look at the next header.
				currentHeader = currentHeader->next;
			}
		}
	}

	userAgent = &(ngx_str_t)ngx_string("");
	for (matchConfIndex = 0;
		matchConfIndex < FIFTYONE_DEGREES_CONFIG_LEVELS;
		matchConfIndex++) {
		currentHeader = matchConf[matchConfIndex]->header;
		while (currentHeader != NULL) {
			if ((currentHeader->multi & (1<<ngx_http_51D_ua_only)) && (int)currentHeader->variableName.len > 0) {
				nextUserAgent = ngx_http_51D_get_user_agent(r, currentHeader);
				if (nextUserAgent != NULL) {
					if (userAgent != NULL &&
						strcmp(
							(const char *)userAgent->data,
							(const char *)nextUserAgent->data) == 0) {
						process(r, fdmcf, currentHeader, h, matchIndex, 1, userAgent);
					}
					else {
						userAgent = nextUserAgent;
						process(r, fdmcf, currentHeader, h, matchIndex, 0, userAgent);
					}
					matchIndex++;
				}
			}
			currentHeader = currentHeader->next;
		}
	}

	// Tell nginx to continue with other module handlers.
	return NGX_DECLINED;
}

/**
 * Find a header in the response header list.
 * @param r a ngx_http_request_t
 * @param headerName header name to find.
 * @return pointer to the header that match.
 */
static ngx_table_elt_t *findResponseHeader(ngx_http_request_t *r, ngx_str_t headerName) {
	ngx_list_part_t *part = &r->headers_out.headers.part;
	ngx_table_elt_t *header = part->elts;
	ngx_uint_t i;
	for (i = 0; /* void */; i++) {
		if (i >= part->nelts) {
			if (part->next == NULL) {
				break;
			}

			part = part->next;
			header = part->elts;
			i = 0;
		}

		if (header[i].hash == 0)  {
			continue;
		}

		if (header[i].key.len == headerName.len &&
			ngx_strcmp(header[i].key.data, headerName.data) == 0) {
			return &header[i];
		}
	}
	return NULL;
}

/**
 * Header filter for response message. This is called first in the header
 * filter change. It then call subsequent filter in the chain with the updated
 * response.
 * @param r the response.
 * @return the status code.
 */
static ngx_int_t
ngx_http_51D_header_filter(ngx_http_request_t *r) {
	ngx_int_t rc;
	ngx_buf_t *b;
	ngx_chain_t out;
	ngx_http_51D_loc_conf_t *fdlcf;
	ngx_http_51D_srv_conf_t *fdscf;
	ngx_http_51D_main_conf_t *fdmcf;
	ngx_http_51D_match_conf_t *lMatchConf, *sMatchConf, *mMatchConf;
	ngx_str_t *userAgent;
	size_t contentLength = 0;
	ngx_uint_t i = 0;

	if (ngx_http_51D_shm_resource_manager == NULL ||
		r->headers_in.headers.last == NULL) {
		return ngx_http_next_header_filter(r);
	}

	// Get 51Degrees configs.
	fdlcf = ngx_http_get_module_loc_conf(r, ngx_http_51D_module);
	fdscf = ngx_http_get_module_srv_conf(r, ngx_http_51D_module);
	fdmcf = ngx_http_get_module_main_conf(r, ngx_http_51D_module);

	lMatchConf = &fdlcf->matchConf;
	sMatchConf = &fdscf->matchConf;
	mMatchConf = &fdmcf->matchConf;

	// Determine if Set Headers should be set.
	// Always prioritise the lower level, so the order is
	// location, server, main.
	ngx_uint_t setHeaders = 0;
	if (lMatchConf->setHeaders == NGX_CONF_UNSET_UINT) {
		if (sMatchConf->setHeaders == NGX_CONF_UNSET_UINT) {
			setHeaders = mMatchConf->setHeaders != NGX_CONF_UNSET_UINT ?
				mMatchConf->setHeaders : 0;
		}
		else {
			setHeaders = sMatchConf->setHeaders;
		}
	}
	else {
		setHeaders = lMatchConf->setHeaders;
	}

	// Setting the headers
	if (setHeaders) {
		ngx_table_elt_t *h;
		ngx_http_51D_data_to_set *currentHeader = fdmcf->setRespHeader;
		for (i = 0; currentHeader != NULL; i++) {
			u_char *escapedValueString = getEscapedMatchedValueString(
				r, fdmcf, currentHeader, i, 0, NULL, ",");
			if (escapedValueString == NULL) {
				return NGX_ERROR;
			}

			ngx_table_elt_t *header = findResponseHeader(r, currentHeader->headerName);

			if (header == NULL) {
				// For each property value pair, set a new header name and value.
				h = ngx_list_push(&r->headers_out.headers);
				h->key.data = (u_char *)currentHeader->headerName.data;
				h->key.len = ngx_strlen(h->key.data);
				h->hash = ngx_hash_key(h->key.data, h->key.len);
				h->value.data = escapedValueString;
				h->value.len = ngx_strlen(h->value.data);
				h->lowcase_key = (u_char *)currentHeader->lowerHeaderName.data;
			}
			else {
				size_t len = header->value.len + ngx_strlen(escapedValueString) + ngx_strlen(",") + 1;
				u_char *newHeaderValue = (u_char *)ngx_palloc(r->pool, len);
				if (newHeaderValue == NULL) {
					report_insufficient_memory_status(r->connection->log);
					return NGX_ERROR;
				}

				ngx_sprintf(
					newHeaderValue,
					"%s%s%s",
					header->value.data,
					",",
					escapedValueString) ;
				newHeaderValue[len] = '\0';
				header->value.data = newHeaderValue;
				header->value.len = len;
			}
			currentHeader = currentHeader->next;
		}
	}

	// Handling 51D_set_javascript_* directives
	if (lMatchConf->body != NULL) {
		userAgent = (lMatchConf->body->multi == 0 &&
			(int)lMatchConf->body->variableName.len <= 0) ||
			lMatchConf->body->multi == 1 ?
			ngx_http_51D_get_user_agent(r, NULL) :
			ngx_http_51D_get_user_agent(r, lMatchConf->body);
		
		memset(fdmcf->valueString, 0, FIFTYONE_DEGREES_MAX_STRING);

		if (lMatchConf->body->propertyCount > 0) {
			// Get a match. If there are multiple instances of
			// 51D_match_single, 51D_match_ua, 51D_match_client_hints or 51D_match_all, then don't get the
			// match if it has already been fetched.
			// If Response Headers was performed and
			// a '51D_get_javascript_all' is used then
			// no need to perform detection again.
			if (lMatchConf->body->multi == 0 ||
				(lMatchConf->body->multi && 
					(setHeaders == 0 || fdmcf->setRespHeaderCount <= 0))) {
				ngx_uint_t ngxCode = ngx_http_51D_get_match(fdmcf, r, lMatchConf->body->multi, userAgent);
				if (ngxCode != NGX_OK) {
					return ngxCode;
				}
			}

			// For each property, set the value in value_string_array.
			ngx_http_51D_get_value(
				fdmcf,
				r,
				fdmcf->valueString,
				(const char *)lMatchConf->body->property[0]->data,
				FIFTYONE_DEGREES_MAX_STRING,
				1,
				NULL);
		}

		// Send header
		contentLength = ngx_strlen(fdmcf->valueString);
		r->headers_out.status = NGX_HTTP_OK;
		if (contentLength > 0 &&
			ngx_strcmp(
				fdmcf->valueString,
				FIFTYONE_DEGREES_PROPERTY_NOT_AVAILABLE) != 0) {
			r->headers_out.content_length_n = contentLength;
		}
		else {
			r->headers_out.content_length_n =
				ngx_strlen(FIFTYONE_DEGREES_JAVASCRIPT_NOT_AVAILABLE);
		}
	}
	return ngx_http_next_header_filter(r);
}

/**
 * Body filter of the response. This is called first in the body filter chain.
 * It then call the subsequent filter in the chain with the updated response
 * and either old or new content, depending on whether a matching is
 * successfully performed.
 * @param r the response.
 * @param in the body chain.
 * @return the status code.
 */
static ngx_int_t
ngx_http_51D_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
	ngx_int_t rc;
	ngx_buf_t *b;
	ngx_chain_t out;
	ngx_http_51D_loc_conf_t *fdlcf;
	ngx_http_51D_main_conf_t *fdmcf;
	ngx_http_51D_match_conf_t *matchConf;
	ngx_str_t *userAgent;
	size_t contentLength = 0;

	if (ngx_http_51D_shm_resource_manager == NULL ||
		r->headers_in.headers.last == NULL) {
		return ngx_http_next_body_filter(r, in);
	}

	// Get 51Degrees configs.
	fdlcf = ngx_http_get_module_loc_conf(r, ngx_http_51D_module);
	fdmcf = ngx_http_get_module_main_conf(r, ngx_http_51D_module);

	matchConf = &fdlcf->matchConf;

	if (matchConf->body != NULL) {
		memset(fdmcf->valueString, 0, FIFTYONE_DEGREES_MAX_STRING);

		// Get the value string for the required property
		// There should only be one property since only
		// content that is supported is a javascript. 
		if (matchConf->body->propertyCount > 0) {
			ngx_http_51D_get_value(
				fdmcf,
				r,
				fdmcf->valueString,
				(const char *)matchConf->body->property[0]->data,
				FIFTYONE_DEGREES_MAX_STRING,
				1,
				NULL);
		}

		// Send body
		b = ngx_calloc_buf(r->pool);
		if (b == NULL) {
			return NGX_ERROR;
		}

		b->last_buf = (r == r->main) ? 1 : 0;
		b->last_in_chain = 1;

		b->memory = 1;

		contentLength = ngx_strlen(fdmcf->valueString);
		if (contentLength > 0 &&
			ngx_strcmp(
				fdmcf->valueString,
				FIFTYONE_DEGREES_PROPERTY_NOT_AVAILABLE) != 0) {
			b->pos = (u_char *) fdmcf->valueString;
		}
		else {
			contentLength = ngx_strlen(FIFTYONE_DEGREES_JAVASCRIPT_NOT_AVAILABLE);
			b->pos = (u_char *) FIFTYONE_DEGREES_JAVASCRIPT_NOT_AVAILABLE;
		}
		b->last = b->pos + contentLength;

		out.buf = b;
		out.next = NULL;

		rc = ngx_http_next_body_filter(r, &out);
	}
	else {
		rc = ngx_http_next_body_filter(r, in);
	}
	return rc;
}

/**
 * Set data function. Initialises the data structure for a given occurrence
 * of "51D_match_single", "51D_match_ua", "51D_match_client_hints", "51D_match_all", "51D_get_javascript_single" or
 * "51D_get_javascript_all" in the config file. Allocates space required and
 * sets the name and properties.
 *
 * @param cf the nginx config.
 * @param data data to be set.
 * @param value the values passed from the config file.
 * @param propertiesString command separated list of properties.
 * @param propertiesCount number of properties in the string.
 * @param fdmcf 51Degrees main config.
 * @return Nginx status code.
 */
static char *
set_data(
	ngx_conf_t *cf,
	ngx_http_51D_data_to_set *data,
	ngx_str_t *value,
	char *propertiesString,
	int propertiesCount,
	ngx_http_51D_main_conf_t *fdmcf)
{
	char *tok, *tokPos = NULL;

	// Allocate space for the properties array.
	data->property =
		(ngx_str_t **)ngx_palloc(cf->pool, sizeof(ngx_str_t *)*propertiesCount);
	if (data->property == NULL) {
		report_insufficient_memory_status(cf->log);
		return NGX_CONF_ERROR;	
	}

	tok = strtok((char *)propertiesString, (const char *)",");
	while (tok != NULL) {
		data->property[data->propertyCount] =
			(ngx_str_t *)ngx_palloc(cf->pool, sizeof(ngx_str_t));
		if (data->property == NULL) {
			report_insufficient_memory_status(cf->log);
			return NGX_CONF_ERROR;
		}

		data->property[data->propertyCount]->data =
			(u_char *)ngx_palloc(cf->pool, sizeof(u_char) * ((ngx_strlen(tok) + 1)));
		if (data->property[data->propertyCount]->data == NULL) {
			report_insufficient_memory_status(cf->log);
			return NGX_CONF_ERROR;
		}

		strcpy(
			(char *)data->property[data->propertyCount]->data,
			(const char *)tok);
		data->property[data->propertyCount]->len =
			ngx_strlen(data->property[data->propertyCount]->data);

		// A is not already included if it does not present
		// in the composed properties string and is not a substring
		// of other already presented properties.
		// e.g. ScreenPixelsWidth is substring of ScreenPixelsWidthJavascript
		// so when checking the present of ScreenPixelsWidth, need to
		// make sure that it is not mistaken with substring of
		// ScreenPixelsWidthJavascript.
		tokPos = ngx_strstr(fdmcf->properties, tok);
		if ((tokPos == NULL ||
			(tokPos != NULL &&
				(tokPos + ngx_strlen(tok))[0] != ',' &&
				(tokPos + ngx_strlen(tok))[0] != '\0')) &&
			 !is_metadata(tok)) {
			add_value(
				",",
				tok,
				fdmcf->properties,
				FIFTYONE_DEGREES_MAX_PROPS_STRING - strlen(fdmcf->properties));
		}
		data->propertyCount++;
		tok = strtok(NULL, ",");
	}

	// Set the variable name or other header if they are specified.
	// If data is for response body, there will be no header.
	// For 51D_match_single, 51D_match_ua, 51D_match_client_hints and 51D_match_all, 
	// number of arguments is from 2 to 3, while
	// for 51D_get_javascript_single and 51D_get_javascript_all
	// it is 1 to 2.
	if ((data->headerName.len == 0 && (int)cf->args->nelts >= 3) ||
		(data->headerName.len > 0 && (int)cf->args->nelts >= 4)) {
		// Body data will only take 2 arguments while header data
		// takes 3
		ngx_uint_t i = data->headerName.len > 0 ? 3 : 2;
		if (value[i].data[0] == '$') {
			data->variableName.data = value[i].data + 1;
			data->variableName.len = value[i].len - 1;
		}
		else {
			ngx_log_error(
				NGX_LOG_ERR,
				cf->cycle->log,
				0,
				"51Degrees argument '%s' is not in a valid format",
				(const char *)value[i].data);
			return NGX_CONF_ERROR;
		}
	}
	else {
		data->variableName = (ngx_str_t)ngx_null_string;
	}
	return NGX_CONF_OK;
}

/**
 * Set up the header to be set.
 * @param cf the nginx config.
 * @param header the header to be set.
 * @param value the value that is passed from the config file.
 * @param fdmcf the 51Degrees main config.
 * @return the status code.
 */
static char*
ngx_http_51D_set_header(
	ngx_conf_t *cf,
	ngx_http_51D_data_to_set *header,
	ngx_str_t *value,
	ngx_http_51D_main_conf_t *fdmcf)
{
	char *tok, *tokPos = NULL;
	int propertiesCount, charPos;
	char *propertiesString;

	// Initialise the property count.
	header->propertyCount = 0;

	// Set the name of the header.
	header->headerName.data = (u_char *)ngx_palloc(cf->pool, value[1].len + 1);
	header->lowerHeaderName.data = (u_char *)ngx_palloc(cf->pool, value[1].len + 1);
	strcpy((char *)header->headerName.data, (char *)value[1].data);
	header->headerName.len = value[1].len;
	ngx_strlow(header->lowerHeaderName.data, header->headerName.data, header->headerName.len);
	header->lowerHeaderName.len = header->headerName.len;

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
	header->property =
		(ngx_str_t **)ngx_palloc(cf->pool, sizeof(ngx_str_t *)*propertiesCount);
	if (header->property == NULL) {
		report_insufficient_memory_status(cf->log);
		return NGX_CONF_ERROR;	
	}

	return set_data(cf, header, value, propertiesString, propertiesCount, fdmcf);
}

/**
 * Set up the body to be set.
 * @param cf the nginx config.
 * @param body to be set.
 * @param value the value that is passed from the config file.
 * @param fdmcf the 51Degrees main config.
 * @return the status code
 */
static char*
ngx_http_51D_set_body(
	ngx_conf_t *cf,
	ngx_http_51D_data_to_set *body,
	ngx_str_t *value,
	ngx_http_51D_main_conf_t *fdmcf)
{
	char *tok, *tokPos = NULL;
	int propertiesCount, charPos;
	char *propertiesString;

	// Initialise the property count.
	body->propertyCount = 0;

	// Default the header name to NULL and 0 length for body data.
	body->headerName.data = NULL;
	body->lowerHeaderName.data = NULL;
	body->headerName.len = 0;
	body->lowerHeaderName.len = 0;

	// Set the properties string to the second argument in the config file.
	propertiesString = (char *)value[1].data;

	// Count the properties in the string.
	propertiesCount = 1;
	for (charPos = 0; charPos < (int)value[1].len; charPos++) {
		if (propertiesString[charPos] == ',') {
			propertiesCount++;
		}
	}

	if (propertiesCount > 1) {
		ngx_log_error(
			NGX_LOG_ERR,
			cf->log,
			0,
			"51Degrees only one property can be used to get Javascript.");
		return NGX_CONF_ERROR;
	}

	// Allocate space for the properties array.
	body->property =
		(ngx_str_t **)ngx_palloc(cf->pool, sizeof(ngx_str_t *)*propertiesCount);
	if (body->property == NULL) {
		report_insufficient_memory_status(cf->log);
		return NGX_CONF_ERROR;	
	}

	return set_data(cf, body, value, propertiesString, propertiesCount, fdmcf);
}

/**
 * Set function. Is called for each occurrence of "51D_match_single", "51D_match_ua", "51D_match_client_hints" or
 * "51D_match_all". Allocates space for the header structure and initialises
 * it with the set header function.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param matchConf the match config.
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_conf_header(
ngx_conf_t *cf, ngx_command_t *cmd, ngx_http_51D_match_conf_t *matchConf)
{
	ngx_http_51D_main_conf_t *fdmcf;
	ngx_http_51D_data_to_set *header;
	ngx_str_t *value;
	char *status;

	// Get the arguments from the config.
	value = cf->args->elts;

	// Get the modules location and main config.
	fdmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_51D_module);

	// Get the end of the header list.
	if (matchConf->header == NULL)
	{
		// Allocate the space for the first header in the current location config.
		matchConf->header =
			(ngx_http_51D_data_to_set*)ngx_palloc(
				cf->pool, sizeof(ngx_http_51D_data_to_set));
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
		// Allocate the space for the next header in the current location config.
		header->next =
			(ngx_http_51D_data_to_set*)ngx_palloc(
				cf->pool, sizeof(ngx_http_51D_data_to_set));
		if (header->next == NULL) {
			report_insufficient_memory_status(cf->log);
			return NGX_CONF_ERROR;
		}

		header = header->next;
	}

	// Set the next pointer to NULL to show it is the last in the list.
	header->next = NULL;

	FILE *f = fopen("_FILTERING.txt", "a");
	fprintf(f, "command: '%s'\n", (const char *)cmd->name.data);

	// Enable multiple HTTP header matching.
	if (ngx_strcmp(cmd->name.data, "51D_match_client_hints") == 0) {
		header->multi = 1<<ngx_http_51D_client_hints;
		matchConf->multiMask |= 1<<ngx_http_51D_client_hints;
	}
	// Enable single User-Agent matching.
	else if (ngx_strcmp(cmd->name.data, "51D_match_single") == 0
		|| ngx_strcmp(cmd->name.data, "51D_match_ua") == 0) {
		header->multi = 1<<ngx_http_51D_ua_only;
		matchConf->multiMask |= 1<<ngx_http_51D_ua_only;
	}
	// Enable multiple HTTP header matching.
	else if (ngx_strcmp(cmd->name.data, "51D_match_all") == 0) {
		header->multi = 1<<ngx_http_51D_all;
		matchConf->multiMask |= 1<<ngx_http_51D_all;
	}

	fprintf(f, "header->multi: '%d'\n", (int)header->multi);
	fclose(f);

	// Set the properties for the selected location.
	status = ngx_http_51D_set_header(cf, header, value, fdmcf);
	if (status != NGX_OK) {
		return status;
	}

	matchConf->headerCount++;

	return NGX_CONF_OK;
}

/**
 * Set function. Is called for each occurrence of "51D_get_javascript_single" or
 * "51D_get_javascript_all". Allocates space for the body structure and
 * initialises it with the set body function.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param matchConf the match config.
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_conf_body(
ngx_conf_t *cf, ngx_command_t *cmd, ngx_http_51D_match_conf_t *matchConf)
{
	ngx_http_51D_main_conf_t *fdmcf;
	ngx_http_51D_data_to_set *body;
	ngx_str_t *value;
	char *status;

	// Get the arguments from the config.
	value = cf->args->elts;

	// Get the modules location and main config.
	fdmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_51D_module);

	// Get the body.
	if (matchConf->body == NULL)
	{
		// Allocate the space for the body in the current location config.
		matchConf->body =
			(ngx_http_51D_data_to_set*)ngx_palloc(
				cf->pool, sizeof(ngx_http_51D_data_to_set));
		if (matchConf->body == NULL) {
			report_insufficient_memory_status(cf->log);
			return NGX_CONF_ERROR;
		}

		body = matchConf->body;
	}
	else {
		ngx_log_error(
			NGX_LOG_ERR,
			cf->log,
			0,
			"51Degrees there can only be one body to set.");
		return NGX_CONF_ERROR;
	}

	// Set the next pointer to NULL to show there is only one body.
	body->next = NULL;

	// Enable single User-Agent matching.
	if (ngx_strcmp(cmd->name.data, "51D_get_javascript_single") == 0) {
		body->multi = 1<<ngx_http_51D_ua_only;
		matchConf->multiMask |= 1<<ngx_http_51D_ua_only;
	}
	// Enable multiple HTTP header matching.
	else if (ngx_strcmp(cmd->name.data, "51D_get_javascript_all") == 0) {
		body->multi = 1<<ngx_http_51D_all;
		matchConf->multiMask |= 1<<ngx_http_51D_all;
	}

	// Set the properties for the selected location.
	status = ngx_http_51D_set_body(cf, body, value, fdmcf);
	if (status != NGX_OK) {
		return status;
	}

	return NGX_CONF_OK;
}

/**
 * Set function. Is called for each occurrence of "51D_set_resp_headers"
 * Allocates space for the body structure and initialises it with the
 * set body function.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param matchConf the match config.
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_conf_resp(
ngx_conf_t *cf, ngx_command_t *cmd, ngx_http_51D_match_conf_t *matchConf)
{
	ngx_http_51D_data_to_set *body;
	ngx_str_t *value;
	char *status;

	// Get the arguments from the config.
	value = cf->args->elts;

	if (value->len > 0 && ngx_strcmp("on", value[1].data) == 0) {
		matchConf->setHeaders = 1;
	}
	else if (value->len > 0 && ngx_strcmp("off", value[1].data) == 0) {
		matchConf->setHeaders = 0;
	}
	else {
		ngx_log_error(
			NGX_LOG_WARN,
			cf->log,
			0,
			"51Degrees incorrect input format value for 51D_set_resp_headers directive. "
			"Value can only be 'on' or 'off'.");
		return NGX_CONF_ERROR;
	}

	return NGX_CONF_OK;
}

/**
 * Set function. Is called for occurrences of "51D_set_resp_headers"
 * in a location config block.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_loc_resp(ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_loc_conf_t *fdlcf =
		ngx_http_conf_get_module_loc_conf(cf, ngx_http_51D_module);
	return ngx_http_51D_set_conf_resp(cf, cmd, &fdlcf->matchConf);
}

/**
 * Set function. Is called for occurrences of "51D_set_resp_headers" 
 * in a server config block.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_srv_resp(ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_srv_conf_t *fdscf =
		ngx_http_conf_get_module_srv_conf(cf, ngx_http_51D_module);
	return ngx_http_51D_set_conf_resp(cf, cmd, &fdscf->matchConf);
}

/**
 * Set function. Is called for occurrences of "51D_set_resp_headers" 
 * in a main config block. 
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_main_resp(ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_main_conf_t *fdmcf =
		ngx_http_conf_get_module_main_conf(cf, ngx_http_51D_module);
	return ngx_http_51D_set_conf_resp(cf, cmd, &fdmcf->matchConf);
}

/**
 * Set function. Is called for occurrences of "51D_get_javascript_single" or
 * "51D_javascript_all" in a location config block. Allocates space for the
 * body structure and initialises it with the set body function.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_loc_cdn(ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_loc_conf_t *fdlcf =
		ngx_http_conf_get_module_loc_conf(cf, ngx_http_51D_module);
	return ngx_http_51D_set_conf_body(cf, cmd, &fdlcf->matchConf);
}

/**
 * Set function. Is called for occurrences of "51D_match_single", "51D_match_ua", "51D_match_client_hints" or
 * "51D_match_all" in a location config block. Allocates space for the
 * header structure and initialises it with the set header function.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_loc(ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_loc_conf_t *fdlcf =
		ngx_http_conf_get_module_loc_conf(cf, ngx_http_51D_module);
	return ngx_http_51D_set_conf_header(cf, cmd, &fdlcf->matchConf);
}

/**
 * Set function. Is called for occurrences of "51D_match_single", "51D_match_ua", "51D_match_client_hints" or
 * "51D_match_all" in a server config block. Allocates space for the
 * header structure and initialises it with the set header function.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_srv(ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_srv_conf_t *fdscf =
		ngx_http_conf_get_module_srv_conf(cf, ngx_http_51D_module);
	return ngx_http_51D_set_conf_header(cf, cmd, &fdscf->matchConf);
}

/**
 * Set function. Is called for occurrences of "51D_match_single", "51D_match_ua", "51D_match_client_hints" or
 * "51D_match_all" in a http config block. Allocates space for the
 * header structure and initialises it with the set header function.
 * @param cf the nginx conf.
 * @param cmd the name of the command called from the config file.
 * @param conf A pointer to the context for configuration object
 * @return char* nginx conf status.
 */
static char *ngx_http_51D_set_main(ngx_conf_t* cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_51D_main_conf_t *fdmcf =
		ngx_http_conf_get_module_main_conf(cf, ngx_http_51D_module);
	return ngx_http_51D_set_conf_header(cf, cmd, &fdmcf->matchConf);
}

/**
 * @}
 */
