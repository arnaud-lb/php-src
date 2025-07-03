--TEST--
Partial application fuzz 004
--FILE--
<?php
function foo($a, $b) {
    return $a + $b;
}

$foo = foo(..., b: 10);

try {
    $foo->__invoke(UNDEFINED);
} catch (\Throwable $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECT--
Undefined constant "UNDEFINED"
