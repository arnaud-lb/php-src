--TEST--
Concrete parameterized types used in type expressions
--FILE--
<?php

abstract class AbstractTest<T> {
    public function method(T $param) {
        var_dump($param);
    }
}

abstract class AbstractPassthru<T> extends AbstractTest<T> {
}

abstract class AbstractDefaulted<T=int> extends AbstractTest<T> {
}

class ConcreteInt extends AbstractTest<int> {
}

class ConcreteString extends AbstractTest<string> {
}

class ConcreteIntPassthru extends AbstractPassthru<int> {
}

class ConcreteIntDefaulted extends AbstractDefaulted {
}
class ConcreteStringDefaulted extends AbstractDefaulted<string> {
}

class ConcretePassthru<T> {
    public function method(T $param) {
        var_dump($param);
    }
}

class ConcreteDefaulted<T=int> {
    public function method(T $param) {
        var_dump($param);
    }
}

function test(AbstractTest<int> $test) {
    $test->method(42);
}

test(new ConcreteInt);
test(new ConcreteIntPassthru);
test(new ConcreteIntDefaulted);
try {
    test(new ConcreteString);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

function test2(AbstractDefaulted $test) {
    $test->method(42);
}

function test3(AbstractDefaulted<int> $test) {
    $test->method(42);
}

test2(new ConcreteIntDefaulted);
test3(new ConcreteIntDefaulted);
try {
    test2(new ConcreteStringDefaulted);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    test3(new ConcreteStringDefaulted);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

function test4(ConcretePassthru<int> $test) {
    $test->method(42);
}

test4(new ConcretePassthru<int>);
try {
    test4(new ConcretePassthru<string>);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

function test5(ConcreteDefaulted $test) {
    $test->method(42);
}

function test6(ConcreteDefaulted<int> $test) {
    $test->method(42);
}

test5(new ConcreteDefaulted);
test5(new ConcreteDefaulted<int>);
test6(new ConcreteDefaulted);
test6(new ConcreteDefaulted<int>);
try {
    test5(new ConcreteDefaulted<string>);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    test6(new ConcreteDefaulted<string>);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECTF--
int(42)
int(42)
int(42)
test(): Argument #1 ($test) must be of type AbstractTest<int>, ConcreteString given, called in %s on line %d
int(42)
int(42)
test2(): Argument #1 ($test) must be of type AbstractDefaulted, ConcreteStringDefaulted given, called in %s on line %d
test3(): Argument #1 ($test) must be of type AbstractDefaulted<int>, ConcreteStringDefaulted given, called in %s on line %d
int(42)
test4(): Argument #1 ($test) must be of type ConcretePassthru<int>, ConcretePassthru given, called in %s on line %d
int(42)
int(42)
int(42)
int(42)
test5(): Argument #1 ($test) must be of type ConcreteDefaulted, ConcreteDefaulted given, called in %s on line %d
test6(): Argument #1 ($test) must be of type ConcreteDefaulted<int>, ConcreteDefaulted given, called in %s on line %d
