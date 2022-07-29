--TEST--
Internal enums dl()
--SKIPIF--
<?php include dirname(__DIR__, 3) . "/ext/dl_test/tests/skip.inc"; ?>
--EXTENSIONS--
opcache
--INI--
opcache.enable_cli=1
--FILE--
<?php

if (extension_loaded('dl_test')) {
    exit('Error: dl_test is already loaded');
}

if (PHP_OS_FAMILY === 'Windows') {
    $loaded = dl('php_dl_test.dll');
} else {
    $loaded = dl('dl_test.so');
}

var_dump($loaded);
--EXPECT--
bool(true)
