--TEST--
with() calls exitContext($exception) on exception and re-throws by default
--FILE--
<?php

require 'basic_manager.inc';

try {
    with (new Manager() as $value) {
        echo "In with() block\n";
        var_dump($value);
        throw new Exception('exception in with block');
    }
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

echo "After with() block\n";

?>
--EXPECTF--
Manager::enterContext()
In with() block
object(stdClass)#%d (0) {
}
Manager::exitContext(Exception(exception in with block))
Exception: exception in with block
After with() block
