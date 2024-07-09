--TEST--
Module declaration with namespace
--FILE--
<?php

require_modules([__DIR__.'/module_decl_with_namespace/module.ini']);

?>
--EXPECT--
Foo
Foo\Bar
Foo\Bar\C
Foo
