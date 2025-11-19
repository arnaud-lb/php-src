--TEST--
Closure application preserves #[SensitiveParameter]
--FILE--
<?php

function f($a, #[SensitiveParameter] $b, $c) {
    throw new Exception();
}

echo "# During partial application:\n";

try {
    $f = f(1, 'sensitive');
} catch (Error $e) {
    echo $e, "\n\n";
}

echo "# In trampoline:\n";

try {
    $f = f(1, ?, ?)('sensitive');
} catch (Error $e) {
    echo $e, "\n\n";
}

echo "# In execution:\n";

try {
    $f = f(1, ?, ?)('sensitive', 3);
} catch (Exception $e) {
    echo $e, "\n";
}

?>
--EXPECTF--
# During partial application:
ArgumentCountError: Too few arguments to function f(), 2 passed in %s on line %d and exactly 3 expected in %s:%d
Stack trace:
#0 %s(%d): f(1, Object(SensitiveParameterValue))
#1 {main}

# In trampoline:
ArgumentCountError: Too few arguments to function {closure:%s:%d}(), 1 passed in %s on line %d and exactly 2 expected in %s:%d
Stack trace:
#0 %s(%d): {closure:%s}(Object(SensitiveParameterValue))
#1 {main}

# In execution:
Exception in %s:%d
Stack trace:
#0 %s(%d): f(1, Object(SensitiveParameterValue), 3)
#1 %s(%d): {closure:%s}(Object(SensitiveParameterValue), 3)
#2 {main}
