--TEST--
using() does not rethrow exception when exitContext() returns true
--FILE--
<?php

require 'basic_manager.inc';

using (new Manager(false) as $value) {
    echo "In using() block\n";
    var_dump($value);
    throw new Exception('exception in using block');
}

echo "After using() block\n";

?>
--EXPECTF--
Manager::enterContext()
In using() block
object(stdClass)#%d (0) {
}
Manager::exitContext(Exception(exception in using block))
After using() block
