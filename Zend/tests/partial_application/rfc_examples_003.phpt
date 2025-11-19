--TEST--
Closure application RFC examples: errors
--FILE--
<?php

if (time() > 0) {
    function stuff(int $i, string $s, float $f, Point $p, int $m = 0) {}
}

try {
    stuff(?);
} catch (Error $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

try {
    stuff(?, ?, ?, ?, ?, ?);
} catch (Error $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

try {
    stuff(?, ?, 3.5, null, i: 5);
} catch (Error $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

?>
--EXPECT--
ArgumentCountError: Partial application of stuff() expects at least 4 arguments, 1 given
ArgumentCountError: Partial application of stuff() expects at most 5 arguments, 6 given
Error: Named parameter $i overwrites previous placeholder
