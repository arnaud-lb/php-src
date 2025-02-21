--TEST--
Module declaration
--FILE--
<?php

require_modules([__DIR__.'/module_decl/module.ini']);

?>
--EXPECT--
Foo
Foo
Foo\C
Foo
