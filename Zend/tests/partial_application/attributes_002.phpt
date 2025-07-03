--TEST--
Partial application preserves #[SensitiveParameter]
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
Error: not enough arguments for application of f, 1 given and exactly 2 expected, declared in %s on line %d in %s:%d
Stack trace:
#0 %s(%d): Closure->f(Object(SensitiveParameterValue))
#1 {main}

# In execution:
Exception in %s:%d
Stack trace:
#0 %s(%d): f(1, Object(SensitiveParameterValue), 3)
#1 {main}
