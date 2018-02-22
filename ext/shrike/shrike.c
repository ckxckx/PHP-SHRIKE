/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2017 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_shrike.h"

/* If you declare any globals in php_shrike.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(shrike)
*/

/* True global resources - no need for thread safety here */
static int le_shrike;

// I think this should be fine as we're not going to be running multiple
// instances of PHP. Docs on that are here
// https://secure.php.net/manual/en/internals2.structure.globals.php

// Indicates whether or not malloc/free/calloc/realloc calls should be logged
int shrike_logging_enabled;
// Indicates whether or not to enable searching for pointers in memory
int shrike_pointer_logging_enabled;

void *shrike_allocated_pointers[10000];
size_t shrike_allocated_sizes[10000];
size_t shrike_allocated_pointers_idx;

int shrike_destination_recording_enabled;
int shrike_source_recording_enabled;

size_t shrike_current_index;
size_t shrike_source_index;
size_t shrike_destination_index;

PHP_FUNCTION(shrike_pointer_sequence_start)
{
	shrike_pointer_logging_enabled = 1;
}

int log_proc_map()
{
	size_t read;
	size_t len;
	char *line = NULL;

	int tmp_logging = shrike_logging_enabled;
	int tmp_pointer_logging = shrike_pointer_logging_enabled;
	shrike_logging_enabled = 0;
	shrike_pointer_logging_enabled = 0;

	FILE *fp = fopen("/proc/self/maps", "r");
	if (!fp) {
		shrike_logging_enabled = tmp_logging;
		shrike_pointer_logging_enabled = tmp_pointer_logging;
		return 1;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		printf("vtx map %s", line);
	}

	fclose(fp);
	if (line) {
		free(line);
	}

	shrike_logging_enabled = tmp_logging;
	shrike_pointer_logging_enabled = tmp_pointer_logging;
	return 0;
}

PHP_FUNCTION(shrike_pointer_sequence_end)
{
	size_t i = 0;

	if (!shrike_pointer_logging_enabled) {
		php_error(E_ERROR, "Pointer logging has not been enabled");
		return;
	}

	if (log_proc_map()) {
		php_error(E_ERROR, "Failed to log /proc/self/maps");
		return;
	}

	for (i = 0; i < shrike_allocated_pointers_idx; ++i) {
		size_t *buf = shrike_allocated_pointers[i];
		size_t size = shrike_allocated_sizes[i];

		if (buf && size >= 8) {
			size_t j = 0;
			for (j = 0; j < size / 8; ++j) {
				size_t content = buf[j];
				// The driver script will check each candidate pointer against
				// the logged process map, but the following check weeds out a
				// lot of data which is obviously not a pointer.
				if (content && content < 0x7fffffffffff && !(content & 0xf)) {
					printf("vtx ptr %lu %lu 0x%" PRIxPTR " 0x%" PRIxPTR "\n",
							size, j * 8, (uintptr_t) buf, (uintptr_t) content);
				}
			}
		}
	}

	shrike_pointer_logging_enabled = 0;
	shrike_allocated_pointers_idx = 0;
	memset(shrike_allocated_pointers, 0x0, 10000 * sizeof(void *));
}

PHP_FUNCTION(shrike_sequence_start)
{
	shrike_logging_enabled = 1;
}

PHP_FUNCTION(shrike_sequence_end)
{
	shrike_logging_enabled = 0;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_shrike_record_destination, 0, 0, 1)
	ZEND_ARG_INFO(0, "destination")
ZEND_END_ARG_INFO()

PHP_FUNCTION(shrike_record_destination)
{
	size_t d = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &d) == FAILURE) {
	    return;
	}

	shrike_destination_recording_enabled = 1;
	shrike_current_index = 0;
	shrike_destination_index = d;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_shrike_record_source, 0, 0, 1)
	ZEND_ARG_INFO(0, "source")
ZEND_END_ARG_INFO()

PHP_FUNCTION(shrike_record_source)
{
	size_t s = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &s) == FAILURE) {
	    return;
	}

	shrike_source_recording_enabled = 1;
	shrike_current_index = 0;
	shrike_source_index = s;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_shrike_alloc_buffer, 0, 0, 1)
	ZEND_ARG_INFO(0, "size")
ZEND_END_ARG_INFO()

PHP_FUNCTION(shrike_alloc_buffer)
{
	size_t sz = 0;
	uint8_t *ptr = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &sz) == FAILURE) {
	    return;
	}

	ptr = emalloc(sz);
	if (!ptr) {
		php_error(E_ERROR, "Failed to allocate buffer");
		RETURN_FALSE;
	}

	RETURN_RES(zend_register_resource(ptr, le_shrike));
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_shrike_write_to_buffer, 0, 0, 1)
	ZEND_ARG_INFO(0, dst)
	ZEND_ARG_INFO(0, src)
	ZEND_ARG_INFO(0, count)
ZEND_END_ARG_INFO()

PHP_FUNCTION(shrike_write_to_buffer)
{
	size_t sz = 0;
	zval *z_src, *z_dst;
	uint8_t *src, *dst;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rrl",
				&z_src, &z_dst, &sz) == FAILURE) {
	    return;
	}

	if ((src = (uint8_t*) zend_fetch_resource(Z_RES_P(z_src), "le_shrike",
				le_shrike)) == NULL) {
		php_error(E_ERROR, "Failed to
		RETURN_FALSE;
	}

	if ((dst = (uint8_t*) zend_fetch_resource(Z_RES_P(z_dst), "le_shrike",
				le_shrike)) == NULL) {
		RETURN_FALSE;
	}

}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(shrike)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(shrike)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(shrike)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "shrike support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ shrike_functions[]
 *
 * Every user visible function must have an entry in shrike_functions[].
 */
const zend_function_entry shrike_functions[] = {
	PHP_FE(shrike_sequence_start, NULL)
	PHP_FE(shrike_sequence_end, NULL)
	PHP_FE(shrike_pointer_sequence_start, NULL)
	PHP_FE(shrike_pointer_sequence_end, NULL)
	PHP_FE(shrike_record_destination, arginfo_shrike_record_destination)
	PHP_FE(shrike_record_source, arginfo_shrike_record_source)
	PHP_FE(shrike_alloc_buffer, arginfo_shrike_alloc_buffer)
	PHP_FE(shrike_write_buffer, arginfo_shrike_write_buffer)
	PHP_FE_END	/* Must be the last line in shrike_functions[] */
};
/* }}} */

/* {{{ shrike_module_entry
 */
zend_module_entry shrike_module_entry = {
	STANDARD_MODULE_HEADER,
	"shrike",
	shrike_functions,
	PHP_MINIT(shrike),
	PHP_MSHUTDOWN(shrike),
	NULL,
	NULL,
	PHP_MINFO(shrike),
	PHP_SHRIKE_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_SHRIKE
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(shrike)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
