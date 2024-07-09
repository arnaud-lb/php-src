<?php

module Test;

if (getenv('THIS_IS_DEFINED')) {
    class A {
        function f() {
            return new class implements I {};
        }
    }
}
