--TEST--
Generic param in static context 004
--FILE--
<?php

class C<T> {
    const T A = 1;
}
--EXPECTF--
Fatal error: Cannot use type parameter "T" in static context in %s on line %d
