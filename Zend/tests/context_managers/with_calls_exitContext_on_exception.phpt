--TEST--
using() calls exitContext($exception) on exception and re-throws by default
--FILE--
<?php

require 'basic_manager.inc';

try {
    using (new Manager() as $value) {
        echo "In using() block\n";
        var_dump($value);
        throw new Exception('exception in using block');
    }
} catch (Exception $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

echo "After using() block\n";

?>
--EXPECTF--
Manager::enterContext()
In using() block
object(stdClass)#%d (0) {
}
Manager::exitContext(Exception(exception in using block))
Exception: exception in using block
After using() block
