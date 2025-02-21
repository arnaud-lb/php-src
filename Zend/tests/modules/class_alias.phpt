--TEST--
Modules: class alias
--FILE--
<?php

spl_autoload_register(function ($class) {
    if (str_starts_with($class, 'Test\\M1')) {
        require_modules([__DIR__.'/class_alias/m1/module.ini']);
    }
    if (str_starts_with($class, 'Test\\M2')) {
        require_modules([__DIR__.'/class_alias/m2/module.ini']);
    }
    if (str_starts_with($class, 'Test\\M3')) {
        require_modules([__DIR__.'/class_alias/m3/module.ini']);
    }
});

new \Test\M3\C();
new ReflectionClass(\Test\M2\J::class);

?>
==DONE==
--EXPECT--
==DONE==
