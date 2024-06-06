--TEST--
Lazy objects: makeLazy calls destructor of pre-existing object, unless SKIP_DESTRUCTOR flag is used
--FILE--
<?php

class C {
    public readonly int $a;

    public function __construct() {
        $this->a = 1;
    }

    public function __destruct() {
        var_dump(__METHOD__);
    }
}

print "# Ghost:\n";

$obj = new C();
print "In makeLazy\n";
ReflectionLazyObjectFactory::makeLazyGhost($obj, function ($obj) {
    var_dump("initializer");
    $obj->__construct();
}, ReflectionLazyObjectFactory::SKIP_DESTRUCTOR);
print "After makeLazy\n";

var_dump($obj->a);
$obj = null;

print "# Virtual:\n";

$obj = new C();
print "In makeLazy\n";
ReflectionLazyObjectFactory::makeLazyProxy($obj, function ($obj) {
    var_dump("initializer");
    return new C();
}, ReflectionLazyObjectFactory::SKIP_DESTRUCTOR);
print "After makeLazy\n";

var_dump($obj->a);
$obj = null;

?>
--EXPECT--
# Ghost:
In makeLazy
After makeLazy
string(11) "initializer"
int(1)
string(13) "C::__destruct"
# Virtual:
In makeLazy
After makeLazy
string(11) "initializer"
int(1)
string(13) "C::__destruct"
