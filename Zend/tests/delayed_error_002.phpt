--TEST--
Delayed errors: Exception thrown while delayed errors are pending
--FILE--
<?php

set_error_handler(function ($errno, $errstr) {
    echo "Warning: $errstr\n";
});

try {
    echo "123abc" + [];
} catch (\Throwable $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

?>
--EXPECT--
Warning: A non-numeric value encountered
TypeError: Unsupported operand types: string + array
