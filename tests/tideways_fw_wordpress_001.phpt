--TEST--
Tideways: Wordpress Support
--FILE--
<?php

require __DIR__ . '/common.php';

function load_template($filename) { usleep(100); }
function do_action($event) { usleep(100); }
function get_header() { usleep(100); }
function get_footer() { usleep(100); }
function get_sidebar() { usleep(100); }

tideways_enable();

load_template("/var/www/wordpress/wp_content/templates/foo/bar.php");
do_action("content");
get_header();
get_footer();
get_sidebar();

tideways_disable();
print_spans(tideways_get_spans());
--EXPECTF--
app: 1 timers - 
view: 1 timers - title=foo/bar.php
event: 1 timers - title=content
php: 1 timers - title=get_header
php: 1 timers - title=get_footer
php: 1 timers - title=get_sidebar
