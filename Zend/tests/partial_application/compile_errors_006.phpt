--TEST--
Partial application compile errors: mix application with unpack (placeholder after)
--FILE--
<?php
foo(...["foo" => "bar"], ...);
?>
--EXPECTF--
Fatal error: Cannot use positional argument after argument unpacking %s on line %d

