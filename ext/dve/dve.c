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
  | Author: Sean Heelan (sean@vertex.re)                                 |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_dve.h"

#define MAX_DVE_BUFFERS 8192

/* True global resources - no need for thread safety here */
static int le_dve;

uint8_t *buffers[MAX_DVE_BUFFERS];
size_t next_buffer_idx;

/* {{{ dve_alloc_buffer
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_dve_alloc_buffer, 0, 0, 1)
	ZEND_ARG_INFO(0, "size")
ZEND_END_ARG_INFO()

PHP_FUNCTION(dve_alloc_buffer)
{
	size_t sz;
	uint8_t *ptr = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &sz) == FAILURE) {
	    return;
	}

	if (next_buffer_idx >= MAX_DVE_BUFFERS) {
		php_error(E_ERROR, "Maximum number of allocated buffers exceeded");
		RETURN_FALSE;
	}

	ptr = emalloc(sz);
	if (!ptr) {
		php_error(E_ERROR, "Failed to allocate buffer");
		RETURN_FALSE;
	}

	buffers[next_buffer_idx] = ptr;
	RETURN_LONG(next_buffer_idx++);
}
/* }}} */

/* {{{ dve_free_buffer
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_dve_free_buffer, 0, 0, 1)
	ZEND_ARG_INFO(0, buf)
ZEND_END_ARG_INFO()

PHP_FUNCTION(dve_free_buffer)
{
	size_t buf_id;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l",
				&buf_id) == FAILURE) {
	    return;
	}

	if (!buffers[buf_id]) {
		php_error(E_ERROR, "Attempting to free already free buffer");
		RETURN_FALSE;
	}

	efree(buffers[buf_id]);
	buffers[buf_id] = NULL;
}
/* }}} */

/* {{{ dve_write_to_buffer
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_dve_write_to_buffer, 0, 0, 1)
	ZEND_ARG_INFO(0, dst)
	ZEND_ARG_INFO(0, src)
	ZEND_ARG_INFO(0, count)
ZEND_END_ARG_INFO()

PHP_FUNCTION(dve_write_to_buffer)
{
	uint8_t *dst;
	char *src;
	size_t src_len, buf_id;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls",
				&buf_id, &src, &src_len) == FAILURE) {
	    return;
	}

	dst = buffers[buf_id];
	if (!dst) {
		php_error(E_ERROR, "Attempting to write to free buffer");
		RETURN_FALSE;
	}

	// Don't copy the trailing NULL
	memcpy(dst, src, src_len);
}
/* }}} */

/* {{{ dve_read_from_buffer
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_dve_read_from_buffer, 0, 0, 1)
	ZEND_ARG_INFO(0, src)
	ZEND_ARG_INFO(0, start_idx)
	ZEND_ARG_INFO(0, count)
ZEND_END_ARG_INFO()

PHP_FUNCTION(dve_read_from_buffer)
{
	size_t start_idx, count, buf_id;
	uint8_t *src;
	zend_string *content;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lll",
				&buf_id, &start_idx, &count) == FAILURE) {
	    return;
	}

	src = buffers[buf_id];
	if (!src) {
		php_error(E_ERROR, "Attempting to read from free buffer");
		RETURN_FALSE;
	}

	content = zend_string_alloc(count, 0);
	if (!content) {
		php_error(E_ERROR, "Failed to allocate destination buffer");
		RETURN_FALSE;
	}

	memcpy(ZSTR_VAL(content), src + start_idx , count);
	ZSTR_VAL(content)[count] = '\0';

	RETURN_STR(content);
}
/* }}} */

/* {{{ php_dve_destroy_resource
*/
static void php_dve_destroy_resource(zend_resource *rsrc)
{
	efree(rsrc->ptr);
}

/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(dve)
{
	le_dve = zend_register_list_destructors_ex(php_dve_destroy_resource, NULL,
			"dve", module_number);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(dve)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(dve)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "dve support", "enabled");
	php_info_print_table_end();

}
/* }}} */

/* {{{ dve_functions[]
 *
 * Every user visible function must have an entry in dve_functions[].
 */
const zend_function_entry dve_functions[] = {
	PHP_FE(dve_alloc_buffer, arginfo_dve_alloc_buffer)
	PHP_FE(dve_write_to_buffer, arginfo_dve_write_to_buffer)
	PHP_FE(dve_read_from_buffer, arginfo_dve_read_from_buffer)
	PHP_FE(dve_free_buffer, arginfo_dve_free_buffer)
	PHP_FE_END	/* Must be the last line in dve_functions[] */
};
/* }}} */

/* {{{ dve_module_entry
 */
zend_module_entry dve_module_entry = {
	STANDARD_MODULE_HEADER,
	"dve",
	dve_functions,
	PHP_MINIT(dve),
	PHP_MSHUTDOWN(dve),
	NULL,
	NULL,
	PHP_MINFO(dve),
	PHP_DVE_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_DVE
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(dve)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
