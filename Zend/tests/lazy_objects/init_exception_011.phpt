--TEST--
Lazy objects: Pre-initialization reference source types are properly handled after initializer exception
--FILE--
<?php

class C {
    public ?C $a;
    public ?C $b;
    public $c;
    public function __construct() {
        unset($this->b);
        throw new \Exception('initializer exception');
    }
}

function test(string $name, object $obj) {
    printf("# %s:\n", $name);

    $r = new ReflectionLazyObjectFactory($obj);
    $r->setProperty('a', null);
    $refA = &$obj->a;
    $r->setProperty('b', null);
    $refB = &$obj->b;

    var_dump($obj);
    try {
        var_dump($obj->c);
    } catch (\Exception $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
    var_dump($obj);

    try {
        // $refA retained its reference source type (except for the virtual
        // case: it is the responsibility of the initializer to propagate
        // pre-initialized properties to the instance)
        $refA = 1;
    } catch (\Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }

    // source type was not duplicated
    unset($obj->a);
    $refA = 1;

    try {
        // $refB retained its reference source type
        $refB = 1;
    } catch (\Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }

    // source type was not duplicated
    unset($obj->b);
    $refB = 1;

}

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
$r = ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
});

test('Ghost', $obj);

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C(null);
});

test('Virtual', $obj);
--EXPECTF--
# Ghost:
object(C)#%d (2) {
  ["a"]=>
  &NULL
  ["b"]=>
  &NULL
}
string(11) "initializer"
Exception: initializer exception
object(C)#%d (2) {
  ["a"]=>
  &NULL
  ["b"]=>
  &NULL
}
TypeError: Cannot assign int to reference held by property C::$a of type ?C
TypeError: Cannot assign int to reference held by property C::$b of type ?C
# Virtual:
object(C)#%d (2) {
  ["a"]=>
  &NULL
  ["b"]=>
  &NULL
}
string(11) "initializer"
Exception: initializer exception
object(C)#%d (2) {
  ["a"]=>
  &NULL
  ["b"]=>
  &NULL
}
TypeError: Cannot assign int to reference held by property C::$a of type ?C
TypeError: Cannot assign int to reference held by property C::$b of type ?C
