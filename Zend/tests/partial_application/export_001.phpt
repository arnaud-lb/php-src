--TEST--
Closure application ast export
--INI--
assert.exception=1
--FILE--
<?php
try {
    assert(0 && foo(?) && foo(new stdClass, ...));
} catch (Error $ex) {
    printf("%s: %s\n", $ex::class, $ex->getMessage());
}
?>
--EXPECT--
AssertionError: assert(0 && foo(?) && foo(new stdClass(), ...))
