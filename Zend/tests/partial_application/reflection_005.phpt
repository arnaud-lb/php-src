--TEST--
Closure application reflection: ReflectionFunction::isClosure() is true for partials
--FILE--
<?php

echo (int)(new ReflectionFunction('sprintf'))->isClosure(), "\n";

echo (int)(new ReflectionFunction(function () {}))->isClosure(), "\n";

echo (int)(new ReflectionFunction(sprintf(?)))->isClosure(), "\n";

?>
--EXPECTF--
0
1
1
