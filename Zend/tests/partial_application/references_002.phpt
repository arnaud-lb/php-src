--TEST--
Partial application references 002
--FILE--
<?php

function foo($a, &$b) {
    $b = 2;
}

$a = [];
$b = &$a[0];

$foo = foo(1, ?);
$foo($b);

var_dump($a, $b);

?>
--EXPECT--
array(1) {
  [0]=>
  &int(2)
}
int(2)
