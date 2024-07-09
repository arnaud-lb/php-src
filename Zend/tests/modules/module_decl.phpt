--TEST--
Module declaration
--FILE--
<?php

module Foo;

echo __MODULE__, "\n";
echo __NAMESPACE__, "\n";
?>
--EXPECT--
Foo
Foo
