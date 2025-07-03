--TEST--
Partial application variation uaf in cleanup unfinished calls
--FILE--
<?php
function test($a){}

try {
    test(1,...)(?);
} catch (Error $ex) {
    echo "OK";
}
?>
--EXPECT--
OK
