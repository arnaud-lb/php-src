--TEST--
Delayed errors: Exception thrown while delayed errors are pending, handler throws
--FILE--
<?php

set_error_handler(function ($errno, $errstr) {
    throw new ErrorException("Warning: $errstr");
});

try {
    echo "123abc" + [];
} catch (\Throwable $e) {
    $prefix = '';
    do {
        echo $prefix, $e::class, ": ", $e->getMessage(), "\n";
        $e = $e->getPrevious();
        $prefix = 'Previous: ';
    } while ($e !== null);
}

?>
--EXPECT--
ErrorException: Warning: A non-numeric value encountered
Previous: TypeError: Unsupported operand types: string + array
