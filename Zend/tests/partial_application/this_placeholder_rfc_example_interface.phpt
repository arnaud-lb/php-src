--TEST--
PFA with $this placeholder: RFC example: interface
--FILE--
<?php

require 'rfc_examples.inc';

interface MyInterface {
    public function method(string $foo): int|string|bool;
}

class ParentClass implements MyInterface {
    public function method(string|array $foo): string {
        return '';
    }
}

class ChildClass extends ParentClass {
    public function method(string|array $bar): string {
        return '';
    }
}

$tests = [
    'MyInterface' => [
        $i = MyInterface::method($this: ?, ...),
        static fn (MyInterface $__this, string $foo): int|string|bool
            => $__this->method($foo),
    ],
    'ParentClass' => [
        $p = ParentClass::method($this: ?, ...),
        static fn (ParentClass $__this, string|array $foo): string
            => $__this->method($foo),
    ],
    'ChildClass' => [
        $c = ChildClass::method($this: ?, ...),
        static fn (ChildClass $__this, string|array $bar): string
            => $__this->method($bar),
    ],
];

check_equivalence($tests);

try {
    // Throws TypeError, since an array is not a valid parameter on the interface,
    // despite being valid on ChildClass::method().
    $i(new ChildClass(), []);
} catch (TypeError $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

try {
    // Throws an Error about an unknown parameter 'bar', because the parameter
    // is called 'foo' on the ParentClass::method().
    $p(new ChildClass(), bar: []);
} catch (Error $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

?>
--EXPECTF--
# MyInterface: Ok
# ParentClass: Ok
# ChildClass: Ok
TypeError: MyInterface::{closure:pfa:%s:23}(): Argument #2 ($foo) must be of type string, array given, called in %s on line %d
Error: Unknown named parameter $bar
