--TEST--
Generic param in static context 003
--FILE--
<?php

class C<T> {
    public static function f($a): T {}
}
--EXPECTF--
Fatal error: Cannot use type parameter "T" in static context in %s on line %d
