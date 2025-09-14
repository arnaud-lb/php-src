--TEST--
Closure application errors: placeholder count errors
--FILE--
<?php
function foo($a, $b, $c) {

}

try {
    foo(?);
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

try {
    foo(?, ?, ?, ?);
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

try {
    property_exists(?);
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}

try {
    usleep(?, ?);
} catch (Error $ex) {
    printf("%s\n", $ex->getMessage());
}
?>
--EXPECT--
not enough arguments or placeholders for application of foo, 1 given and exactly 3 expected
too many arguments or placeholders for application of foo, 4 given and a maximum of 3 expected
not enough arguments or placeholders for application of property_exists, 1 given and exactly 2 expected
too many arguments or placeholders for application of usleep, 2 given and a maximum of 1 expected
