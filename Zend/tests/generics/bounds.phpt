--TEST--
Bounds
--FILE--
<?php

class A {}
class B extends A {}
class C<T:A> {}

var_dump(new C<A>());
var_dump(new C<B>());
try {
    new C<stdClass>();
} catch (Error $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

?>
--EXPECT--
object(C<A>)#1 (0) {
}
object(C<B>)#1 (0) {
}
TypeError: Class C: Generic type argument #0 (T) must be of type A, stdClass given
