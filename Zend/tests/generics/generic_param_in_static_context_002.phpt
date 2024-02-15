--TEST--
Generic param in static context 002
--FILE--
<?php

class C<T> {
    public static function f(T $a) {}
}
--EXPECTF--
Fatal error: Cannot use type parameter "T" in static context in %s on line %d
