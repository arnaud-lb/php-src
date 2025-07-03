--TEST--
Partial application reflection: ReflectionFunction::isPartial() is true for partials
--FILE--
<?php

echo (int)(new ReflectionFunction('sprintf'))->isPartial(), "\n";

echo (int)(new ReflectionFunction(function () {}))->isPartial(), "\n";

echo (int)(new ReflectionFunction(sprintf(?)))->isPartial(), "\n";

?>
--EXPECTF--
0
0
1
