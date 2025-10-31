--TEST--
with() handles Throwable
--FILE--
<?php

require 'basic_manager.inc';

try {
    with (new Manager() as $value) {
        echo "In with() block\n";
        var_dump($value);
        throw new Error('error in with block');
    }
} catch (Error $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

echo "After with() block\n";

?>
--EXPECTF--
Manager::enterContext()
In with() block
object(stdClass)#%d (0) {
}
Manager::exitContext(Error(error in with block))
Error: error in with block
After with() block
