--TEST--
Closure application errors: missing parameters
--FILE--
<?php
function foo($a, ...$b) {

}

function bar($a, $b, $c) {}

$foo = foo(?);

try {
    $foo();
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

$foo = foo(?, ?);

try {
    $foo(1);
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

$bar = bar(?, ?, ...);

try {
    $bar(1);
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

class Foo {
    public function bar($a, ...$b) {}
}

$foo = new Foo;

$bar = $foo->bar(?);

try {
    $bar();
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

$repeat = str_repeat('a', ...);

try {
    $repeat();
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

$usleep = usleep(?);

try {
    $usleep();
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

try {
    $usleep(1, 2);
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}
?>
--EXPECTF--
Too few arguments to function {closure:%s:%d}(), 0 passed in %s on line %d and exactly 1 expected
Too few arguments to function {closure:%s:%d}(), 1 passed in %s on line %d and exactly 2 expected
Too few arguments to function {closure:%s:%d}(), 1 passed in %s on line %d and exactly 3 expected
Too few arguments to function Foo::{closure:%s:%d}(), 0 passed in %s on line %d and exactly 1 expected
Too few arguments to function {closure:%s:%d}(), 0 passed in %s on line %d and exactly 1 expected
Too few arguments to function {closure:%s:%d}(), 0 passed in %s on line %d and exactly 1 expected
