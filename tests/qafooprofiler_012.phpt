--TEST--
XHPRof: Test Twig+Smarty Support
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

class Smarty_Internal_TemplateBase
{
    public function fetch($template)
    {
    }
}

class Smarty_Internal_Template extends Smarty_Internal_TemplateBase
{
    protected $template_resource = 'foo.tpl';
}

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

qafooprofiler_enable(
    0,
    array('argument_functions' => array(
        'Twig_Template::display',
        'Smarty_Internal_TemplateBase::fetch'
    ))
);

$template = new Twig_Template();
$template->display(array('foo' => 'bar'));

$smarty3 = new Smarty_Internal_Template();
$smarty3->fetch("bar.tpl");
$smarty3->fetch(NULL);

print_canonical(qafooprofiler_disable());
?>
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>Smarty_Internal_TemplateBase::fetch#bar.tpl: ct=       1; wt=*;
main()==>Smarty_Internal_TemplateBase::fetch#foo.tpl: ct=       1; wt=*;
main()==>Twig_Template::display#test.twig: ct=       1; wt=*;
main()==>Twig_Template::getTemplateName : ct=       1; wt=*;
main()==>qafooprofiler_disable          : ct=       1; wt=*;
