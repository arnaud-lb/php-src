--TEST--
Delayed errors: Stack growth exception
--FILE--
<?php

class Dtor {
    function __destruct() {
        throw new Exception(__METHOD__);
    }
}

function f() {
    new Dtor();
    g();
}

function g() {
    g();
}

try {
    echo "INIT_FCALL\n";
    f();
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

class C {
    function f($name) {
        new Dtor();
        $this->$name();
    }
    function g($name) {
        $this->$name();
    }
}

try {
    echo "INIT_METHOD_CALL\n";
    $c = new C();
    $c->f(zend_test_rc_string('g'));
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

class D {
    static function f($cname, $fname) {
        new Dtor();
        $cname::$fname();
    }
    static function g($cname, $fname) {
        $cname::$fname();
    }
}

try {
    echo "INIT_STATIC_METHOD_CALL\n";
    D::f(zend_test_rc_string('D'), zend_test_rc_string('g'));
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

function uf($f) {
    new Dtor();
    call_user_func($f);
}

function ug($f) {
    call_user_func($f, $f);
}

try {
    echo "INIT_USER_CALL\n";
    uf(zend_test_rc_string('ug'));
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

class E {
    function __construct() {
        new E();
    }
}

function e() {
    new Dtor();
    new E();
}

try {
    echo "NEW\n";
    e();
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

function i() {
    new Dtor();
    include __DIR__ . '/delayed_error_007.inc';
}

try {
    echo "INCLUDE_OR_EVAL\n";
    i();
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

function d($f) {
    new Dtor();
    $f();
}

function df($f) {
    $f($f);
}

try {
    echo "INIT_DYNAMIC_CALL\n";
    d(zend_test_rc_string('df'));
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

?>
==DONE==
--EXPECT--
INIT_FCALL
Exception: Dtor::__destruct
INIT_METHOD_CALL
Exception: Dtor::__destruct
INIT_STATIC_METHOD_CALL
Exception: Dtor::__destruct
INIT_USER_CALL
Exception: Dtor::__destruct
NEW
Exception: Dtor::__destruct
INCLUDE_OR_EVAL
Exception: Dtor::__destruct
INIT_DYNAMIC_CALL
Exception: Dtor::__destruct
==DONE==
