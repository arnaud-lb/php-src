--TEST--
Partial application references 002
--FILE--
<?php

function foo($a, &$b) {
    $a = 2;
}

$foo = foo(1, ?);

try {
    $foo(2);
} catch (\Throwable $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECT--
foo(): Argument #1 ($b) could not be passed by reference
