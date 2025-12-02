--TEST--
using() restores CV
--FILE--
<?php

require 'basic_manager.inc';

class A {}

$value = new A;
$value2 = $value;

echo "# \$value before using():\n";
var_dump($value);

using (new Manager() as $value) {
    echo "# \$value in using():\n";
    var_dump($value);
}

echo "# \$value after using():\n";
var_dump($value);
var_dump($value === $value2);

?>
--EXPECTF--
# $value before using():
object(A)#%d (0) {
}
Manager::enterContext()
# $value in using():
object(stdClass)#%d (0) {
}
Manager::exitContext(null)
# $value after using():
object(A)#%d (0) {
}
bool(true)
