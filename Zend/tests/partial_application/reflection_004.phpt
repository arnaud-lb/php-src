--TEST--
Closure application reflection: ReflectionFunction::isClosure() is true for partials
--FILE--
<?php

echo (int)(new ReflectionFunction('sprintf'))->isClosure(), "\n";

echo (int)(new ReflectionFunction(sprintf(?)))->isClosure(), "\n";

?>
--EXPECT--
0
1
