--TEST--
Modules: classes with invalid dependencies are not allowed
--FILE--
<?php

require __DIR__ . '/helpers.inc';

try {
    require_modules([__DIR__.'/check_deps_002/module.ini']);
} catch (Error $e) {
    printf("%s: %s\n", $e::class, $e->getMessage());
}

?>
==DONE==
--EXPECTF--
Error: Class Test\I not found while compiling module Test%s
==DONE==
