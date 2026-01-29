--TEST--
using() can be immediately followed by any statement
--FILE--
<?php

require 'basic_manager.inc';

using (new Manager() as $value) if (true) {
    echo "In using() block\n";
    var_dump($value);
}

echo "After using() block\n";
var_dump(isset($value));

?>
--EXPECTF--
Manager::enterContext()
In using() block
object(stdClass)#%d (0) {
}
Manager::exitContext(null)
After using() block
bool(false)
