--TEST--
Modules: classes that are never declared are not checked
--ENV--
THIS_IS_DEFINED=1
--FILE--
<?php

require_modules([__DIR__.'/check_deps_001/module.ini']);

?>
==DONE==
--EXPECT--
==DONE==
