<?php

module Test;

// Early bound, and may not have a DECLARE opcode
class A {}

interface I {}

if (getenv('THIS_IS_DEFINED')) {
    // Declared at runtime
    class B {
        function f() {
            // Anon class is usable
            return new class implements I {};
        }
        function g() {
            // Anon function is usable
            return function (): I {};
        }
    }
}

// Early bound
function f() {}

if (getenv('THIS_IS_DEFINED')) {
    // Declared at runtime
    function g() {}
}

if (getenv('THIS_IS_NOT_DEFINED')) {
    // Never declared (should not generate an error)
    class C implements J {
        function f() {
            // Anon class is not usable (should not generate an error)
            return new class implements J {};
        }
        function g() {
            // Anon function is not usable (should not generate an error)
            return function (): J {};
        }

    }
}
