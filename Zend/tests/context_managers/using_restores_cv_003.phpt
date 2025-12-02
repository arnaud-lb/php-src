--TEST--
using() restores CV and preserves references
--FILE--
<?php

require 'basic_manager.inc';

class A {}
class B {}

$value = new A;
$value2 = &$value;

echo "# \$value before using():\n";
var_dump($value);

using (new Manager() as $value) {
    echo "# \$value in using():\n";
    var_dump($value);
    $value = null;
}

echo "# \$value after using():\n";
var_dump($value);
$value = new B;

echo "# \$value2 references \$value:\n";
var_dump($value2 === $value);

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
# $value2 references $value:
bool(true)
