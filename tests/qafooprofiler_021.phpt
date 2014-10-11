--TEST--
XHProf: Curl Summary
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

$options = array('argument_functions' => array('curl_exec'));

qafooprofiler_enable(0, $options);

$ch = curl_init('http://localhost');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
$response = curl_exec($ch);

print_canonical(qafooprofiler_disable());
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>curl_exec#http://localhost#6   : ct=       1; wt=*;
main()==>curl_init                      : ct=       1; wt=*;
main()==>curl_setopt                    : ct=       1; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
