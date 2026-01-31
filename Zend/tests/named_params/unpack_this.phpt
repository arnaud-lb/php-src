--TEST--
Unpacking named parameters: this
--FILE--
<?php

function f(...$args) {
    var_dump($args);
}

f(...['this' => 'test']);

?>
--EXPECT--
array(1) {
  ["this"]=>
  string(4) "test"
}
