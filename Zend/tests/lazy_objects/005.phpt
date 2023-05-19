--TEST--
Lazy objects: initializer must return the right type
--FILE--
<?php

class B {
}

class C extends B {
    public int $a;

    public function __construct() {
        $this->a = 1;
    }
}

class D extends C {
}

/*
print "# Ghost initializer must return NULL or no value:\n";

$obj = (new ReflectionClass(C::class))->newInstanceWithoutConstructor();
ReflectionLazyObject::makeLazy($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
    return new stdClass;
});

var_dump($obj);
try {
    var_dump($obj->a);
} catch (\Error $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}
var_dump($obj);
 */

print "# Virtual initializer must return an instance of a compatible class:\n";

$tests = [
    [C::class, new C()],
    [C::class, new D()],
    [D::class, new C()],
];

foreach ($tests as [$class, $instance]) {
    $obj = (new ReflectionClass($class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::makeLazy($obj, function ($obj) use ($instance) {
        var_dump("initializer");
        return $instance;
    }, ReflectionLazyObject::STRATEGY_VIRTUAL);

    var_dump($obj->a);
    var_dump($obj);
}

$tests = [
    [C::class, new B()],
    [C::class, new stdClass],
    [C::class, new DateTime()],
    [C::class, null],
];

foreach ($tests as [$class, $instance]) {
    $obj = (new ReflectionClass($class))->newInstanceWithoutConstructor();
    ReflectionLazyObject::makeLazy($obj, function ($obj) use ($instance) {
        var_dump("initializer");
        return $instance;
    }, ReflectionLazyObject::STRATEGY_VIRTUAL);

    try {
        var_dump($obj->a);
    } catch (\Error $e) {
        printf("%s: %s\n", $e::class, $e->getMessage());
    }
}
--EXPECT--
# Virtual initializer must return an instance of a compatible class:
string(11) "initializer"
int(1)
object(C)#5 (1) {
  ["instance"]=>
  object(C)#1 (1) {
    ["a"]=>
    int(1)
  }
}
string(11) "initializer"
int(1)
object(C)#6 (1) {
  ["instance"]=>
  object(D)#2 (1) {
    ["a"]=>
    int(1)
  }
}
string(11) "initializer"
int(1)
object(D)#4 (1) {
  ["instance"]=>
  object(C)#3 (1) {
    ["a"]=>
    int(1)
  }
}
string(11) "initializer"
Error: Virtual object intializer was expected to return an instance of C or a parent with the same properties, B returned
string(11) "initializer"
Error: Virtual object intializer was expected to return an instance of C or a parent with the same properties, stdClass returned
string(11) "initializer"
Error: Virtual object intializer was expected to return an instance of C or a parent with the same properties, DateTime returned
string(11) "initializer"
Error: Virtual object intializer was expected to return an instance of C or a parent with the same properties, null returned
