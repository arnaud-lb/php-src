--TEST--
Module declaration with namespace
--FILE--
<?php

module Foo;
namespace Bar;

echo __MODULE__, "\n";
echo __NAMESPACE__, "\n";
?>
--EXPECT--
Foo
Foo\Bar
