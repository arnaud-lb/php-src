--TEST--
Lazy objects: Destructor exception in makeLazy
--FILE--
<?php

class C {
    public readonly int $a;

    public function __construct() {
        $this->a = 1;
    }

    public function __destruct() {
        throw new \Exception(__METHOD__);
    }
}

print "# Ghost:\n";

$obj = new C();
try {
    ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
        var_dump("initializer");
        $obj->__construct();
    });
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

// Object was not made lazy
var_dump(ReflectionLazyObjectFactory::isInitialized($obj));

print "# Virtual:\n";

$obj = new C();
try {
    ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
        var_dump("initializer");
        return new C();
    });
} catch (\Exception $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

// Object was not made lazy
var_dump(ReflectionLazyObjectFactory::isInitialized($obj));

?>
--EXPECT--
# Ghost:
Exception: C::__destruct
bool(true)
# Virtual:
Exception: C::__destruct
bool(true)
