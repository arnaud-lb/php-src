--TEST--
Closure application RFC examples: unary function example
--FILE--
<?php

$input = ['A', 'B', 'C', 'D'];
$legal = ['a', 'B', 'c', 'D'];

$result = array_map(in_array(?, $legal, strict: true), $input);

var_dump($result);

?>
--EXPECT--
array(4) {
  [0]=>
  bool(false)
  [1]=>
  bool(true)
  [2]=>
  bool(false)
  [3]=>
  bool(true)
}
