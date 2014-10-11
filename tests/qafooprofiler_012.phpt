--TEST--
XHPRof: Test Twig Support
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

class Twig_Template
{
    public function getTemplateName()
    {
        return 'test.twig';
    }

    public function display($variables)
    {
    }
}

qafooprofiler_enable(0, array('argument_functions' => array('Twig_Template::display')));

$template = new Twig_Template();
$template->display(array('foo' => 'bar'));

print_canonical(qafooprofiler_disable());
?>
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>Twig_Template::display#test.twig: ct=       1; wt=*;
main()==>Twig_Template::getTemplateName : ct=       1; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
