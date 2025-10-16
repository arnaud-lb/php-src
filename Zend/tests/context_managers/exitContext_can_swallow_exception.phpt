--TEST--
with() does not rethrow exception when exitContext() returns true
--FILE--
<?php

require 'basic_manager.inc';

with (new Manager(false) as $value) {
    echo "In with() block\n";
    var_dump($value);
    throw new Exception('exception in with block');
}

echo "After with() block\n";

?>
--EXPECTF--
Manager::enterContext()
In with() block
object(stdClass)#%d (0) {
}
Manager::exitContext(Exception(exception in with block))
After with() block
