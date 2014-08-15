--TEST--
XHProf: Stream Summary
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

$options = array('argument_functions' => array('fopen', 'fgets', 'fclose'));

xhprof_enable(0, $options);

$fh = fopen(__DIR__, "r");
fgets($fh);
fclose($fh);

$fh = fopen("http://qafoo.com/?foo=bar", "r");
fgets($fh);
fclose($fh);

$fh = fopen("http://php.net", "r");
fgets($fh);
fclose($fh);

print_canonical(xhprof_disable());
--EXPECTF--
main()                                  : ct=       1; wt=*;
main()==>fclose#%d                       : ct=       1; wt=*;
main()==>fclose#%d                       : ct=       1; wt=*;
main()==>fclose#%d                       : ct=       1; wt=*;
main()==>fgets#%d                        : ct=       1; wt=*;
main()==>fgets#%d                        : ct=       1; wt=*;
main()==>fgets#%d                        : ct=       1; wt=*;
main()==>fopen#%s#%d: ct=       1; wt=*;
main()==>fopen#http://php.net#%d         : ct=       1; wt=*;
main()==>fopen#http://qafoo.com/#%d      : ct=       1; wt=*;
main()==>xhprof_disable                 : ct=       1; wt=*;
