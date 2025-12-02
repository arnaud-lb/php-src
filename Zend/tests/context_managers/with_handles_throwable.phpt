--TEST--
using() handles Throwable
--FILE--
<?php

require 'basic_manager.inc';

try {
    using (new Manager() as $value) {
        echo "In using() block\n";
        var_dump($value);
        throw new Error('error in using block');
    }
} catch (Error $e) {
    echo $e::class, ": ", $e->getMessage(), "\n";
}

echo "After using() block\n";

?>
--EXPECTF--
Manager::enterContext()
In using() block
object(stdClass)#%d (0) {
}
Manager::exitContext(Error(error in using block))
Error: error in using block
After using() block
