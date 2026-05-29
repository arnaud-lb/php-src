--TEST--
Delayed errors: Promoted errors take precedence
--FILE--
<?php

set_error_handler(function ($errno, $errstr) {
    throw new ErrorException("Warning: $errstr");
}, delay: false);

class Test {
    static function test() {
        /* Throws ErrorException(Warning: Use of "static" in callables is deprecated),
         * followed by TypeError(Argument #1 ($callback) must be a valid callback).
         * The first one takes precedence. */
        call_user_func("static::ok");
    }
    static function ok() {
    }
}

try {
    Test::test();
} catch (\Throwable $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

?>
--EXPECT--
ErrorException: Warning: Use of "static" in callables is deprecated
