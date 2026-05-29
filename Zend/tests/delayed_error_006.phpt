--TEST--
Delayed errors: Promoted errors take precedence
--FILE--
<?php

set_error_handler(function ($errno, $errstr) {
    if (error_reporting() & $errno) {
        throw new Exception($errstr, $errno);
    }
}, delay: false);

@trigger_error('Test', E_USER_WARNING);

?>
==DONE==
--EXPECT--
==DONE==
