--TEST--
using() restores CV iff it was set
--FILE--
<?php

require 'basic_manager.inc';

using (new Manager() as $value) {
    var_dump($value);
}

echo "\$value is set after using():\n";
var_dump(isset($value));
var_dump(array_key_exists('value', get_defined_vars()));

?>
--EXPECTF--
Manager::enterContext()
object(stdClass)#%d (0) {
}
Manager::exitContext(null)
$value is set after using():
bool(false)
bool(false)
