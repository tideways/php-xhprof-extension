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

#ifndef PHP_QAFOOPROFILER_H
#define PHP_QAFOOPROFILER_H

extern zend_module_entry qafooprofiler_module_entry;
#define phpext_qafooprofiler_ptr &qafooprofiler_module_entry

#ifdef PHP_WIN32
#define PHP_QAFOOPROFILER_API __declspec(dllexport)
#else
#define PHP_QAFOOPROFILER_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(qafooprofiler);
PHP_MSHUTDOWN_FUNCTION(qafooprofiler);
PHP_RINIT_FUNCTION(qafooprofiler);
PHP_RSHUTDOWN_FUNCTION(qafooprofiler);
PHP_MINFO_FUNCTION(qafooprofiler);

PHP_FUNCTION(qafooprofiler_enable);
PHP_FUNCTION(qafooprofiler_disable);
PHP_FUNCTION(qafooprofiler_sample_enable);
PHP_FUNCTION(qafooprofiler_layers_enable);
PHP_FUNCTION(qafooprofiler_last_fatal_error);
PHP_FUNCTION(qafooprofiler_last_exception_data);

#endif	/* PHP_QAFOOPROFILER_H */
