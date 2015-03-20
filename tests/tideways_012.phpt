--TEST--
XHPRof: Test Twig+Smarty Support
Author: beberlei
--FILE--
<?php

include_once dirname(__FILE__).'/common.php';

class Smarty
{
    public function fetch($template)
    {
    }
}

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

tideways_enable(
    0,
    array('argument_functions' => array(
        'Twig_Template::display',
        'Smarty::fetch',
        'Smarty_Internal_TemplateBase::fetch',
    ))
);

$template = new Twig_Template();
$template->display(array('foo' => 'bar'));

$smarty3 = new Smarty_Internal_Template();
$smarty3->fetch("bar.tpl");
$smarty3->fetch(NULL);

$smarty2 = new Smarty();
$smarty2->fetch("foo.tpl");
$smarty2->fetch(NULL);

print_canonical(tideways_disable());
?>
--EXPECT--
main()                                  : ct=       1; wt=*;
main()==>Smarty::fetch#                 : ct=       1; wt=*;
main()==>Smarty::fetch#foo.tpl          : ct=       1; wt=*;
main()==>Smarty_Internal_TemplateBase::fetch#bar.tpl: ct=       1; wt=*;
main()==>Smarty_Internal_TemplateBase::fetch#foo.tpl: ct=       1; wt=*;
main()==>Twig_Template::display#test.twig: ct=       1; wt=*;
main()==>Twig_Template::getTemplateName : ct=       1; wt=*;
main()==>tideways_disable               : ct=       1; wt=*;
