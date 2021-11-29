--TEST--
Workaround for PHP Bug #81430
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80000) {
    echo "skip requires 8 due to attributes";
}
--FILE--
<?php
include __DIR__ . "/common.php";

#[\Attribute]
class A {
    private $foo;
    public function __construct($foo)
    {
        $this->foo = $foo;
    }
}

#[A("baz")]
class B {
    public function getBar($input) { return $input * 2; }
}

#[A("bar")]
function B() {}

tideways_xhprof_enable();

$foo = new A("foo");

$b = new B();
var_dump($b->getBar(4));

$r = new \ReflectionFunction("B");
var_dump($r->getAttributes(A::class)[0]->newInstance());
var_dump(call_user_func([$r->getAttributes(A::class)[0], 'newInstance']));

$output = tideways_xhprof_disable();

print_canonical($output);
--EXPECTF--
int(8)
object(A)#5 (1) {
  ["foo":"A":private]=>
  string(3) "bar"
}
object(A)#4 (1) {
  ["foo":"A":private]=>
  string(3) "bar"
}
call_user_func==>ReflectionAttribute::newInstance: ct=       1; wt=*;
main()                                  : ct=       1; wt=*;
main()==>B::getBar                      : ct=       1; wt=*;
main()==>ReflectionAttribute::newInstance: ct=       1; wt=*;
main()==>ReflectionFunction::__construct: ct=       1; wt=*;
main()==>ReflectionFunctionAbstract::getAttributes: ct=       2; wt=*;
main()==>call_user_func                 : ct=       1; wt=*;
main()==>tideways_xhprof_disable        : ct=       1; wt=*;
main()==>var_dump                       : ct=       3; wt=*;
