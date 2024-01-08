--TEST--
Test instantiation of class with unbound params
--FILE--
<?php

class Test1<T> {
    public function method(T $param) {
        var_dump($param);
    }
}

class Test2<T,U> {
    public function method(T $t, U $u) {
        var_dump($t, $u);
    }
}

class Test3<T=int> {
    public function method(T $param) {
        var_dump($param);
    }
}

class AbstractTest4<T,U> {
    public function method1(T $t, U $u) {
        var_dump($t, $u);
    }
}

class Test4<T> extends AbstractTest4<T,string> {
    public function method2(T $param) {
        var_dump($param);
    }
}

$test = new Test1<int>();
$test->method(42);
try {
    $test->method([]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

echo "\n";

$test = new Test2<int,string>();
$test->method(42,'hello');
try {
    $test->method([],'hello');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    $test->method(42,[]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

echo "\n";

$test = new Test3();
$test->method(42);
try {
    $test->method([]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

echo "\n";

$test = new Test3<string>();
$test->method('hello');
try {
    $test->method([]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

echo "\n";

$test = new Test4<int>();
$test->method1(42, 'hello');
$test->method2(42);
try {
    $test->method1([], 'hello');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    $test->method1(42,[]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
try {
    $test->method2([]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
--EXPECTF--
int(42)
Test1::method(): Argument #1 ($param) must be of type T (where T = int), array given, called in %s on line %d

int(42)
string(5) "hello"
Test2::method(): Argument #1 ($t) must be of type T (where T = int), array given, called in %s on line %d
Test2::method(): Argument #2 ($u) must be of type U (where U = string), array given, called in %s on line %d

int(42)
Test3::method(): Argument #1 ($param) must be of type T (where T = int), array given, called in %s on line %d

string(5) "hello"
Test3::method(): Argument #1 ($param) must be of type T (where T = string), array given, called in %s on line %d

int(42)
string(5) "hello"
int(42)
AbstractTest4::method1(): Argument #1 ($t) must be of type T (where T = int), array given, called in %s on line %d
AbstractTest4::method1(): Argument #2 ($u) must be of type U (where U = string), array given, called in %s on line %d
Test4::method2(): Argument #1 ($param) must be of type T (where T = int), array given, called in %s on line %d
