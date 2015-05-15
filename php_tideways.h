/*
 *  Copyright (c) 2009 Facebook
 *  Copyright (c) 2014 Qafoo GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef PHP_TIDEWAYS_H
#define PHP_TIDEWAYS_H

extern zend_module_entry tideways_module_entry;
#define phpext_tideways_ptr &tideways_module_entry

#ifdef PHP_WIN32
#define PHP_TIDEWAYS_API __declspec(dllexport)
#else
#define PHP_TIDEWAYS_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(tideways);
PHP_MSHUTDOWN_FUNCTION(tideways);
PHP_RINIT_FUNCTION(tideways);
PHP_RSHUTDOWN_FUNCTION(tideways);
PHP_MINFO_FUNCTION(tideways);

PHP_FUNCTION(tideways_enable);
PHP_FUNCTION(tideways_disable);
PHP_FUNCTION(tideways_transaction_name);
PHP_FUNCTION(tideways_fatal_backtrace);
PHP_FUNCTION(tideways_prepend_overwritten);
PHP_FUNCTION(tideways_last_detected_exception);
PHP_FUNCTION(tideways_last_fatal_error);
PHP_FUNCTION(tideways_sql_minify);

PHP_FUNCTION(tideways_span_create);
PHP_FUNCTION(tideways_get_spans);
PHP_FUNCTION(tideways_span_timer_start);
PHP_FUNCTION(tideways_span_timer_stop);
PHP_FUNCTION(tideways_span_annotate);
PHP_FUNCTION(tideways_span_watch);
PHP_FUNCTION(tideways_callback_watch);

#endif	/* PHP_TIDEWAYS_H */
