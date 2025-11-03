--TEST--
Closure application references 004
--FILE--
<?php

function foo(&$a, $b) {
}

try {
    $foo = foo(1, ?);
} catch (\Throwable $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECT--
foo(): Argument #1 ($a) could not be passed by reference
