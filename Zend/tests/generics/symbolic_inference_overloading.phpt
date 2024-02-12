--TEST--
Symbolic inference with overloading
--EXTENSIONS--
gmp
--FILE--
<?php

require __DIR__ . '/symbolic_inference.inc';

function arithmetic() {
    var_dump(new C(_int() + _gmp()));
    var_dump(new C(_int() * _gmp()));
    var_dump(new C(_gmp() ^ _int()));
    var_dump(new C(_gmp() | _int()));
}

foreach (get_defined_functions()['user'] as $function) {
    if (str_starts_with($function, '_')) {
        continue;
    }

    printf("\n# %s\n", $function);

    $function();
}

?>
--EXPECT--
# arithmetic
object(C<GMP>)#1 (0) {
}
object(C<GMP>)#1 (0) {
}
object(C<GMP>)#1 (0) {
}
object(C<GMP>)#1 (0) {
}
