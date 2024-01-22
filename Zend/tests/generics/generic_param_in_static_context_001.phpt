--TEST--
Generic param in static context 001
--FILE--
<?php

class C<T> {
    public static T $a;
}
--EXPECTF--
Fatal error: Cannot use type parameter "T" in static context in %s on line %d
