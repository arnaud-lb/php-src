--TEST--
using() calls enter and exit
--FILE--
<?php

require 'basic_manager.inc';

using (new Manager() as $value) {
    echo "In using() block\n";
    var_dump($value);
}

echo "After using() block\n";

?>
--EXPECTF--
Manager::enterContext()
In using() block
object(stdClass)#%d (0) {
}
Manager::exitContext(null)
After using() block
