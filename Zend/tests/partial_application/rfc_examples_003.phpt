--TEST--
Partial application RFC examples: errors
--FILE--
<?php

if (time() > 0) {
    function stuff(int $i, string $s, float $f, Point $p, int $m = 0) {}
}

try {
    stuff(?);
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}

try {
    stuff(?, ?, ?, ?, ?, ?);
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}

try {
    stuff(?, ?, 3.5, null, i: 5);
} catch (Error $e) {
    echo $e->getMessage(), "\n";
}

--EXPECTF--
not enough arguments or placeholders for application of stuff, 1 given and at least 4 expected, declared in %s on line %d
too many arguments or placeholders for application of stuff, 6 given and a maximum of 5 expected, declared in %s on line %d
Named parameter $i overwrites previous placeholder
