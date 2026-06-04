--TEST--
Stream hook: closed during read hook
--FILE--
<?php

stream_set_hook(function ($stream) {
    fclose($stream);
    stream_set_hook(null);
});

$fd = fopen(__FILE__, 'r');
var_dump(fgets($fd));
?>
--EXPECTF--
Warning: fclose(): cannot close the provided stream, as it must not be manually closed in %s on line %d
string(6) "<?php
"
