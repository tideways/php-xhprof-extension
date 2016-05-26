<?php

/**
 * Print xhprof raw data (essentially a callgraph) in a canonical style,
 * so that even if the ordering of things within the raw_data (which is
 * an associative array) changes in future implementations the output
 * remains stable.
 *
 * Also, suppress output of variable quantities (such as time, memory)
 * so that reference output of tests doesn't need any additional masking.
 *
 * @author Kannan
 */
function print_canonical($xhprof_data)
{
    // some functions are not part of the trace
    // due to being an opcode in php7, these
    // need to be ignored for a common test-output
    $ignoreFunctions = array('strlen', 'is_array');

    ksort($xhprof_data);
    foreach($xhprof_data as $func => $metrics) {
        foreach ($ignoreFunctions as $ignoreFunction) {
            if (strpos($func, "==>" . $ignoreFunction) !== false) {
                continue 2;
            }
        }

        echo str_pad($func, 40) . ":";
        ksort($metrics);
        foreach ($metrics as $name => $value) {

            // Only call counts are stable.
            // Wild card everything else. We still print
            // the metric name to ensure it was collected.
            if ($name != "ct") {
                $value = "*";
            } else {
                $value = str_pad($value, 8, " ", STR_PAD_LEFT);
            }

            echo " {$name}={$value};";
        }
        echo "\n";
    }
}


function print_spans($spans)
{
    foreach ($spans as $span) {
        if (!isset($span['a'])) {
            $span['a'] = array();
        }

        ksort($span['a']);
        $annotations = '';
        foreach ($span['a'] as $k => $v) {
            if ($k === 'fn') { continue; }
            $annotations .= "$k=$v ";
        }

        printf("%s: %d timers - %s\n", $span['n'], count($span['b']), rtrim($annotations));
    }
}

/**
 * Code is from https://github.com/php/php-src/blob/master/ext/curl/tests/server.inc licensed under PHP license
 */

define ("PHP_HTTP_SERVER_HOSTNAME", "localhost");
define ("PHP_HTTP_SERVER_PORT", 8964);
define ("PHP_HTTP_SERVER_ADDRESS", PHP_HTTP_SERVER_HOSTNAME.":".PHP_HTTP_SERVER_PORT);

function http_cli_server_start() {
    if(getenv('PHP_HTTP_HTTP_REMOTE_SERVER')) {
        return getenv('PHP_HTTP_HTTP_REMOTE_SERVER');
    }

    $php_executable = getenv('TEST_PHP_EXECUTABLE');
    $doc_root = __DIR__;
    $router = "http_responder.php";

    $descriptorspec = array(
        0 => STDIN,
        1 => STDOUT,
        2 => STDERR,
    );

    if (substr(PHP_OS, 0, 3) == 'WIN') {
        $cmd = "{$php_executable} -t {$doc_root} -n -S " . PHP_HTTP_SERVER_ADDRESS;
        $cmd .= " {$router}";
        $handle = proc_open(addslashes($cmd), $descriptorspec, $pipes, $doc_root, NULL, array("bypass_shell" => true,  "suppress_errors" => true));
    } else {
        $cmd = "exec {$php_executable} -t {$doc_root} -n -S " . PHP_HTTP_SERVER_ADDRESS;
        $cmd .= " {$router}";
        $cmd .= " 2>/dev/null";

        $handle = proc_open($cmd, $descriptorspec, $pipes, $doc_root);
    }

    // note: even when server prints 'Listening on localhost:8964...Press Ctrl-C to quit.'
    //       it might not be listening yet...need to wait until fsockopen() call returns
    $i = 0;
    while (($i++ < 30) && !($fp = @fsockopen(PHP_HTTP_SERVER_HOSTNAME, PHP_HTTP_SERVER_PORT))) {
        usleep(10000);
    }

    if ($fp) {
        fclose($fp);
    }

    register_shutdown_function(
        function($handle) use($router) {
            proc_terminate($handle);
        },
            $handle
        );
    // don't bother sleeping, server is already up
    // server can take a variable amount of time to be up, so just sleeping a guessed amount of time
    // does not work. this is why tests sometimes pass and sometimes fail. to get a reliable pass
    // sleeping doesn't work.
    return PHP_HTTP_SERVER_ADDRESS;
}
