--TEST--
Tideways: Wordpress Support
--FILE--
<?php

require __DIR__ . '/common.php';

function load_template($filename) { }
function do_action($event) { }
function get_header() { }
function get_footer() { }
function get_sidebar() { }

tideways_enable();

load_template("/var/www/wordpress/wp_content/templates/foo/bar.php");
do_action("content");
get_header();
get_footer();
get_sidebar();

print_spans(tideways_get_spans());
--EXPECTF--
view: 1 timers - title=foo/bar.php
event: 1 timers - title=content
php.wordpress: 1 timers - title=get_header
php.wordpress: 1 timers - title=get_footer
php.wordpress: 1 timers - title=get_sidebar
