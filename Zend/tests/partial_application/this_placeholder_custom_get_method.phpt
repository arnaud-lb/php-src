--TEST--
PFA with $this placeholder - custom get_method()
--FILE--
<?php

try {
    // Closure::__invoke is resolvable only via obj->handlers->get_method()
    $f = Closure::__invoke(this: ?);
} catch (Error $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

// SplFileObject implements a custom get_method() handler, but methods can be
// resolved by zend_std_get_static_method()
$f = SplFileObject::ftell(this: ?);
var_dump($f(new SplFileObject(__FILE__)));

?>
--EXPECT--
Error: Method Closure::__invoke() does not exist or does not support partial application
int(0)
