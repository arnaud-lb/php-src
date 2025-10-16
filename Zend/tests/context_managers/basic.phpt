--TEST--
with() calls enter and exit
--FILE--
<?php

require 'basic_manager.inc';

with (new Manager() as $value) {
    echo "In with() block\n";
    var_dump($value);
}

echo "After with() block\n";

?>
--EXPECTF--
Manager::enterContext()
In with() block
object(stdClass)#%d (0) {
}
Manager::exitContext(null)
After with() block
